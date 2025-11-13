#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "pipeline_cache.hpp"

#include "pipeline_block.hpp"
#include "utils.cpp"

#include "fifo_block.cpp"
#include "approximate_lru_block.cpp"
#include "cost_aware_lfu_block.cpp"

using Json = nlohmann::json;

IPipelineCache::~IPipelineCache() = default;

PipelineCache::PipelineCache() : m_cache_capacity(0), m_quantum_size(0), m_num_of_quanta(0) {}
PipelineCache::PipelineCache(const std::string& config_path) : PipelineCache(false, config_path) {}

PipelineCache::PipelineCache(const PipelineCache& other) : m_cache_capacity {other.m_cache_capacity},
                                                           m_quantum_size{other.m_quantum_size},
                                                           m_items{other.m_items},
                                                           m_blocks{},
                                                           m_quanta_alloc{other.m_quanta_alloc},
                                                           m_eviction_queue{},
                                                           m_num_of_quanta(other.m_num_of_quanta),
                                                           m_sketch{other.m_sketch},
                                                           m_aging_window_size(other.m_aging_window_size),
                                                           m_ops_since_last_aging{0},
                                                           m_stats{}
{
    m_blocks.resize(other.m_blocks.size());

    for (size_t i = 0; i < other.m_blocks.size(); ++i) {
        const std::string block_type = other.m_blocks[i]->get_type();

        if (block_type == "FIFO") {
            m_blocks[i] = std::make_unique<FIFOBlock>(*dynamic_cast<FIFOBlock*>(other.m_blocks[i].get()));
        } else if (block_type == "ALRU") {
            m_blocks[i] = std::make_unique<ALRUBlock>(*dynamic_cast<ALRUBlock*>(other.m_blocks[i].get()));
        } else if (block_type == "CostAwareLFU") {
            m_blocks[i] = std::make_unique<CostAwareLFUBlock>(*dynamic_cast<CostAwareLFUBlock*>(other.m_blocks[i].get()));
        } else {
            throw std::runtime_error("Unknown block type during copy: " + block_type);
        }
    }

    validate_sizes();
}

PipelineCache& PipelineCache::operator=(const PipelineCache& other)
{
    assert(this != &other);
    assert(!other.m_blocks.empty());

    m_cache_capacity = other.m_cache_capacity;
    m_quantum_size = other.m_quantum_size;
    m_num_of_quanta = other.m_num_of_quanta;

    m_items = other.m_items;

    m_blocks.resize(other.m_blocks.size());

    for (size_t i = 0; i < other.m_blocks.size(); ++i) {
        const std::string block_type = other.m_blocks[i]->get_type();

        if (block_type == "FIFO") {
            m_blocks[i] = std::make_unique<FIFOBlock>(*dynamic_cast<FIFOBlock*>(other.m_blocks[i].get()));
        } else if (block_type == "ALRU") {
            m_blocks[i] = std::make_unique<ALRUBlock>(*dynamic_cast<ALRUBlock*>(other.m_blocks[i].get()));
        } else if (block_type == "CostAwareLFU") {
            m_blocks[i] = std::make_unique<CostAwareLFUBlock>(*dynamic_cast<CostAwareLFUBlock*>(other.m_blocks[i].get()));
        } else {
            throw std::runtime_error("Unknown block type during copy assignment: " + block_type);
        }

        assert(m_blocks[i]->capacity() == other.m_blocks[i]->capacity());
        assert(m_blocks[i]->size() == other.m_blocks[i]->size());
    }

    m_quanta_alloc = other.m_quanta_alloc;
    m_eviction_queue = std::vector<EntryData>();
    m_sketch = other.m_sketch;
    m_ops_since_last_aging = 0;
    m_stats = {};

    validate_sizes();

    return *this;
}

const EntryData& PipelineCache::get_item(uint64_t key)
{
    assert(contains(key));
    const EntryPosition& pos = m_items[key];
    assert(pos.id == key);

    EntryData* item_entry = m_blocks[pos.block_num]->get_entry(pos.idx);

    item_entry->last_access_time = utils::get_current_time_in_ms();
    m_sketch.add(key);
    ++m_ops_since_last_aging;
    age_sketch_if_needed();
    ++m_stats.ops;

    return *item_entry;
}

void PipelineCache::insert_item(uint64_t key, double latency, uint64_t tokens)
{
    EntryData item{key, latency, tokens};
    m_sketch.add(key);
    ++m_ops_since_last_aging;
    m_stats.aggregated_cost += latency * static_cast<double>(tokens);
    ++m_stats.ops;

    bool was_item_evicted = true;
    for (size_t idx = 0; idx < m_blocks.size() && was_item_evicted; ++idx)
    {
        if (m_quanta_alloc[idx] > 0)
        {
            if (InsertionResult result = m_blocks[idx]->insert_item(item);
                result.was_item_inserted)
            {
                assert(result.replaced_idx < m_blocks[idx]->capacity());
                m_items.insert_or_assign(item.id, EntryPosition{item.id, idx, result.replaced_idx});

                was_item_evicted = result.removed_entry.has_value();
                if (was_item_evicted)
                {
                    item = *result.removed_entry;
                }
            }
        }
    }

    if (item.id != key && was_item_evicted)
    {
        m_items.erase(item.id);

        m_eviction_queue.push_back(item);
    }

    validate_sizes();
    age_sketch_if_needed();
}

PipelineCache::PipelineCache(bool is_sampled, const std::string& config_path)
    : m_cache_capacity{0},
      m_quantum_size{0},
      m_items{},
      m_blocks{},
      m_quanta_alloc{},
      m_eviction_queue{},
      m_num_of_quanta{0},
      m_sketch{},
      m_aging_window_size{0},
      m_ops_since_last_aging{0},
      m_stats{}
{
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "ERROR: Failed to open config file: " << config_path << "\n";
        exit(1);
    }

    Json config = Json::parse(config_file);

    const auto& blocks_config = config["blocks"];
    const size_t num_blocks = blocks_config.size();

    m_blocks.resize(num_blocks);
    m_quanta_alloc.resize(num_blocks);

    try
    {
        m_num_of_quanta = config["cache"]["num_of_quanta"].get<uint64_t>();
        if (!utils::is_power_of_two(m_num_of_quanta))
        {
            std::cerr << "num_of_quanta is not a power of 2" << std::endl;
            exit(1);
        }
        m_cache_capacity = config["cache"]["capacity"].get<uint64_t>();
        if (!utils::is_power_of_two(m_cache_capacity))
        {
            std::cerr << "cache_capacity is not a power of 2" << std::endl;
            exit(1);
        }
        const uint64_t non_sampled_quantum_size = m_cache_capacity / m_num_of_quanta;
        const uint64_t sample_rate = config["cache"]["sample_rate"].get<uint64_t>();
        if (!utils::is_power_of_two(sample_rate))
        {
            std::cerr << "sample_rate is not a power of 2" << std::endl;
            exit(1);
        }

        const uint64_t aging_window_multiplier = config["cache"]["aging_window_multiplier"].get<uint64_t>();
        m_aging_window_size = aging_window_multiplier * m_cache_capacity;

        m_quantum_size = !is_sampled
                             ? non_sampled_quantum_size
                             : non_sampled_quantum_size / sample_rate;

        const uint64_t seed = config["cache"]["seed"].get<uint64_t>();
        const uint64_t sample_size = config["cache"]["sample_size"].get<uint64_t>();

        const double sketch_error = config["count_min_sketch"]["error"].get<double>();
        const double sketch_error_probability = config["count_min_sketch"]["probability"].get<double>();
        m_sketch = CountMinSketch{sketch_error, sketch_error_probability, seed};

        for (size_t i = 0; i < num_blocks; ++i)
        {
            const auto& block_config = blocks_config[i];
            const std::string block_type = block_config["type"];
            const uint64_t initial_quanta = block_config["initial_quanta"].get<uint64_t>();

            m_quanta_alloc[i] = initial_quanta;
            if (initial_quanta < 0 || initial_quanta > m_num_of_quanta)
            {
                std::cerr << "The initial quanta of " << i << " is not valid" << std::endl;
                exit(1);
            }

            if (block_type == "fifo")
            {
                m_blocks[i] = std::make_unique<FIFOBlock>(m_cache_capacity, m_quantum_size, initial_quanta);
            }
            else if (block_type == "alru")
            {
                m_blocks[i] = std::make_unique<ALRUBlock>(m_cache_capacity,
                                                          m_quantum_size,
                                                          initial_quanta,
                                                          seed,
                                                          sample_size);
            }
            else if (block_type == "cost_aware_lfu")
            {
                m_blocks[i] = std::make_unique<CostAwareLFUBlock>(m_cache_capacity,
                                                                  m_quantum_size,
                                                                  initial_quanta,
                                                                  m_sketch,
                                                                  seed,
                                                                  sample_size);
            }
            else
            {
                std::cerr << "ERROR: Unknown block type: " << block_type << "\n";
                exit(1);
            }
        }
    }
    catch (const Json::exception& e) {
        std::cerr << "ERROR: Failed to read cache config: " << e.what() << "\n";
        exit(1);
    }


    config_file.close();
}

void PipelineCache::age_sketch_if_needed()
{
    if (m_ops_since_last_aging >= m_aging_window_size)
    {
        m_ops_since_last_aging = 0;
        m_sketch.reduce();
    }
}

void PipelineCache::validate_sizes() const
{
    uint64_t num_of_items = 0;
    for (size_t idx = 0; idx < m_blocks.size(); ++idx)
    {
        const uint64_t blk_size = m_blocks[idx]->size();
        const uint64_t blk_capacity = m_blocks[idx]->capacity();
        assert(blk_size <= m_quanta_alloc[idx] * m_quantum_size);
        assert(blk_capacity == m_quanta_alloc[idx] * m_quantum_size);
        num_of_items += blk_size;
    }

    assert(num_of_items <= m_cache_capacity);
    assert(size() == num_of_items);
    assert(m_items.size() == num_of_items);
}

bool PipelineCache::contains(uint64_t key) const 
{
    return m_items.contains(key);
}

EntryData PipelineCache::evict_item() 
{
    assert(!m_eviction_queue.empty());
    EntryData item = m_eviction_queue.back();
    m_eviction_queue.pop_back();
    return item;
}

bool PipelineCache::should_evict() const
{
    return !m_eviction_queue.empty();
}

void PipelineCache::move_quantum(uint64_t src_block, uint64_t dest_block)
{
    for (const auto & m_block : m_blocks)
    {
        m_block->prepare_for_copy();
    }
    assert(can_adapt(src_block, false) && can_adapt(dest_block, true));
    QuantumMoveResult result = m_blocks[src_block]->move_quanta_to(*m_blocks.at(dest_block));

    // Update positions for items that moved to destination block
    for (const auto& [id, idx] : result.items_moved)
    {
        m_items.insert_or_assign(id, EntryPosition{id, dest_block, idx});
    }

    // Update positions for items that remained in source block (indices may have changed due to rearrangement)
    for (const auto& [id, idx] : result.items_remaining)
    {
        m_items.insert_or_assign(id, EntryPosition{id, src_block, idx});
    }

    --m_quanta_alloc[src_block];
    ++m_quanta_alloc[dest_block];
}

std::vector<uint64_t> PipelineCache::keys() const 
{
    std::vector<uint64_t> res{size()};
    for (const auto& val : m_items | std::views::values)
    {
        res.emplace_back(val.id);
    }

    return res;
}

std::vector<std::tuple<double, uint64_t>> PipelineCache::values() const
{
    std::vector<std::tuple<double, uint64_t>> res{size()};
    for (const auto& val : m_items | std::views::values)
    {
        const EntryData* data = m_blocks[val.block_num]->get_entry(val.idx);
        res.emplace_back(data->latency, data->tokens);
    }

    return res;
}

size_t PipelineCache::capacity() const 
{
    return m_cache_capacity;
}

size_t PipelineCache::size() const
{
    size_t curr_size = 0;
    for (const auto & m_block : m_blocks)
    {
        curr_size += m_block->size();
    }

    return curr_size;
}

bool PipelineCache::empty() const 
{
    return size() == 0;
}


void PipelineCache::clear()
{
    for (const auto & m_block : m_blocks)
    {
        m_block->clear();
    }
}

bool PipelineCache::can_adapt(uint64_t block_num, bool increase) const 
{
    return increase 
           ? m_quanta_alloc[block_num] < m_num_of_quanta
           : m_quanta_alloc[block_num] > 0;
}

std::string PipelineCache::get_current_config() const
{
    std::stringstream ss;

    for (size_t i = 0; i < m_blocks.size(); ++i) {
        if (i > 0) {
            ss << ", ";
        }
        ss << m_blocks[i]->get_type() << ": " << m_quanta_alloc[i];
    }
    ss << "\n";

    return ss.str();
}

double PipelineCache::get_timeframe_aggregated_cost() const { return m_stats.get_average_cost(); }
void PipelineCache::reset_timeframe_stats() { m_stats.reset(); }

void PipelineCache::prepare_for_copy()
{
    for (auto& block : m_blocks)
    {
        block->prepare_for_copy();
    }
}


PipelineCacheProxy::PipelineCacheProxy() {}

PipelineCacheProxy::PipelineCacheProxy(const std::string& config_path)
                : IPipelineCache(),
                  m_cache{true, config_path},
                  is_in_dummy_mode{false} {}

PipelineCacheProxy::PipelineCacheProxy(const PipelineCacheProxy& other)
                : m_cache{other.m_cache},
                  is_in_dummy_mode{false} {}

PipelineCacheProxy& PipelineCacheProxy::operator=(const PipelineCacheProxy& other)
{
    m_cache = other.m_cache;
    is_in_dummy_mode = false;
    return *this;
}

const EntryData& PipelineCacheProxy::get_item(uint64_t key) 
{
    static EntryData dummy;
    return is_in_dummy_mode ? dummy : m_cache.get_item(key);
}

void PipelineCacheProxy::insert_item(uint64_t key, double latency, uint64_t tokens) 
{
    if (!is_in_dummy_mode)
    {
        m_cache.insert_item(key, latency, tokens);
    }
}

bool PipelineCacheProxy::contains(uint64_t key) const 
{
    return is_in_dummy_mode ? false : m_cache.contains(key);
}

EntryData PipelineCacheProxy::evict_item() 
{
    return is_in_dummy_mode ? EntryData() : m_cache.evict_item();
}

bool PipelineCacheProxy::should_evict() const
{
    return !is_in_dummy_mode && m_cache.should_evict();
}

void PipelineCacheProxy::move_quantum(uint64_t src_block, uint64_t dest_block) 
{
    if (!is_in_dummy_mode)
    { 
        m_cache.move_quantum(src_block, dest_block);
    }
}

std::vector<uint64_t> PipelineCacheProxy::keys() const 
{
    return is_in_dummy_mode ? std::vector<uint64_t>{} : m_cache.keys();
}

std::vector<std::tuple<double, uint64_t>> PipelineCacheProxy::values() const 
{
    return is_in_dummy_mode ? std::vector<std::tuple<double, uint64_t>>{} : m_cache.values();
}

size_t PipelineCacheProxy::capacity() const 
{
    return is_in_dummy_mode ? 0 : m_cache.capacity();
}

size_t PipelineCacheProxy::size() const 
{
    return is_in_dummy_mode ? 0 : m_cache.size();
}

bool PipelineCacheProxy::empty() const 
{
    return !is_in_dummy_mode && m_cache.empty();
}

void PipelineCacheProxy::clear() 
{
    if (!is_in_dummy_mode)
    { 
        m_cache.clear();
    }
}

bool PipelineCacheProxy::can_adapt(uint64_t block_num, bool increase) const 
{
    return !is_in_dummy_mode && m_cache.can_adapt(block_num, increase);
}

double PipelineCacheProxy::get_timeframe_aggregated_cost() const 
{
    return is_in_dummy_mode ? std::numeric_limits<double>::max() : m_cache.get_timeframe_aggregated_cost();
}

void PipelineCacheProxy::reset_timeframe_stats() 
{
    if (!is_in_dummy_mode)
    {
        m_cache.reset_timeframe_stats();
    }
}

void PipelineCacheProxy::make_dummy()
{
    is_in_dummy_mode = true;
}

void PipelineCacheProxy::make_non_dummy()
{
    is_in_dummy_mode = false;
}

void PipelineCacheProxy::prepare_for_copy()
{
    if (!is_in_dummy_mode)
    {
        m_cache.prepare_for_copy();
    }
}

