#include <cassert>
#include <cstdint>
#include <limits>
#include <sstream>
#include <iostream>

#include "constants.hpp"
#include "pipeline_cache.hpp"
#include "fixed_size_array.hpp"
#include "pipeline_block.hpp"
#include "utils.cpp"

uint64_t PipelineCache::UID = 0;

IPipelineCache::~IPipelineCache() = default;

PipelineCache::PipelineCache() : PipelineCache(false) {}

PipelineCache::PipelineCache(bool is_sampled)
                            : m_cache_capacity {!is_sampled ? Constants::PIPELINE_CACHE_CAPACITY : Constants::SAMPLED_CACHE_CAPACITY},
                              m_quantum_size{!is_sampled ? Constants::QUANTUM_SIZE : Constants::SAMPLED_QUANTUM_SIZE},
                              m_items{m_cache_capacity * 2},
                              m_blocks{},
                              m_quanta_alloc{2, 6, 8},
                              m_eviction_queue{},
                              m_sketch{Constants::SKETCH_ERROR, Constants::SKETCH_PROB},
                              m_ops_since_last_aging(0),
                              m_stats{},
                              uid{UID++}
{
    const uint64_t quantum_size = !is_sampled ? Constants::QUANTUM_SIZE : Constants::SAMPLED_QUANTUM_SIZE;
    m_blocks[0] = std::make_unique<FIFOBlock>(m_cache_capacity, quantum_size, m_quanta_alloc[0]);
    m_blocks[1] = std::make_unique<ALRUBlock>(m_cache_capacity, quantum_size, m_quanta_alloc[1]);
    m_blocks[2] = std::make_unique<CostAwareLFUBlock>(m_cache_capacity, quantum_size, m_quanta_alloc[2], m_sketch);
    ++UID;
}

PipelineCache::PipelineCache(const PipelineCache& other) : m_cache_capacity {other.m_cache_capacity},
                                                           m_quantum_size{other.m_quantum_size},
                                                           m_items{other.m_items},
                                                           m_blocks{},
                                                           m_quanta_alloc{other.m_quanta_alloc},
                                                           m_eviction_queue{},
                                                           m_sketch{other.m_sketch},
                                                           m_ops_since_last_aging{0},
                                                           m_stats{},
                                                           uid{UID++}
{
    m_blocks[0] = std::make_unique<FIFOBlock>(*dynamic_cast<FIFOBlock*>(other.m_blocks[0].get()));
    m_blocks[1] = std::make_unique<ALRUBlock>(*dynamic_cast<ALRUBlock*>(other.m_blocks[1].get()));
    m_blocks[2] = std::make_unique<CostAwareLFUBlock>(*dynamic_cast<CostAwareLFUBlock*>(other.m_blocks[2].get()));

    validate_sizes();
    ++UID;
}

PipelineCache& PipelineCache::operator=(const PipelineCache& other)
{
    assert(this != &other);
    assert(m_cache_capacity == other.m_cache_capacity);
    assert(m_quantum_size == other.m_quantum_size);
    
    m_items = other.m_items;
    
    m_blocks[0] = std::make_unique<FIFOBlock>(*dynamic_cast<FIFOBlock*>(other.m_blocks[0].get()));
    m_blocks[1] = std::make_unique<ALRUBlock>(*dynamic_cast<ALRUBlock*>(other.m_blocks[1].get()));
    m_blocks[2] = std::make_unique<CostAwareLFUBlock>(*dynamic_cast<CostAwareLFUBlock*>(other.m_blocks[2].get()));

    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS; ++idx)
    {
        assert(m_blocks[idx]->capacity() == other.m_blocks[idx]->capacity());
        assert(m_blocks[idx]->size() == other.m_blocks[idx]->size());
    }

    m_quanta_alloc = other.m_quanta_alloc;
    m_eviction_queue = std::queue<EntryData>{};
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
    m_stats.aggregated_cost += latency * tokens;
    ++m_stats.ops;

    bool was_item_evicted = true;
    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS && was_item_evicted; ++idx)
    {
        if (m_quanta_alloc[idx] > 0)
        {
            auto res = m_blocks[idx]->insert_item(item);
            if (res.first != std::numeric_limits<uint64_t>::max())
            {
                assert(res.first < m_blocks[idx]->capacity());
                m_items.insert_or_assign(item.id, EntryPosition{item.id, idx, res.first});

                was_item_evicted = res.second.has_value();
                if (was_item_evicted)
                {
                    item = *res.second;
                }
            }
        }
    }

    if (item.id != key && was_item_evicted)
    {
        m_items.erase(item.id);

        m_eviction_queue.push(item);
    }

    validate_sizes();
    age_sketch_if_needed();
}

void PipelineCache::age_sketch_if_needed()
{
    if (m_ops_since_last_aging >= Constants::AGING_WINDOW_SIZE * m_cache_capacity)
    {
        m_ops_since_last_aging = 0;
        m_sketch.reduce();
    }
}

void PipelineCache::validate_sizes() const
{
    uint64_t num_of_items = 0;
    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS; ++idx) 
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
    return m_items.find(key) != m_items.end();
}

EntryData PipelineCache::evict_item() 
{
    assert(!m_eviction_queue.empty());
    EntryData item = m_eviction_queue.front();
    m_eviction_queue.pop();
    return item;
}

bool PipelineCache::should_evict() const
{
    return !m_eviction_queue.empty();
}

void PipelineCache::move_quantum(uint64_t src_block, uint64_t dest_block)
{
    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS; ++idx)
    {
        m_blocks[idx]->prepare_for_copy();
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
    for (const auto& item : m_items) 
    {
        const EntryData* data = m_blocks[item.second.block_num]->get_entry(item.second.idx);
        res.emplace_back(item.second.id);
    }

    return res;
}

std::vector<std::tuple<double, uint64_t>> PipelineCache::values() const
{
    std::vector<std::tuple<double, uint64_t>> res{size()};
    for (const auto& item : m_items) 
    {
        const EntryData* data = m_blocks[item.second.block_num]->get_entry(item.second.idx);
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
    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS; ++idx)
    {   
        curr_size += m_blocks[idx]->size();
    }

    return curr_size;
}

bool PipelineCache::empty() const 
{
    return size() == 0;
}


void PipelineCache::clear()
{
    for (uint64_t idx = 0; idx < Constants::NUM_OF_BLOCKS; ++idx)
    {
        m_blocks[idx]->clear();
    }
}

bool PipelineCache::can_adapt(uint64_t block_num, bool increase) const 
{
    return increase 
           ? m_quanta_alloc[block_num] < Constants::NUM_OF_QUONTA 
           : m_quanta_alloc[block_num] > 0;
}

std::string PipelineCache::get_current_config() const
{
    std::stringstream ss;

    ss << "FIFO: " << m_quanta_alloc[0] << ", ALRU: " << m_quanta_alloc[1] << ", CA-LFU: " << m_quanta_alloc[2] << "\n";

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


PipelineCacheProxy::PipelineCacheProxy()
                : IPipelineCache(),
                  m_cache{true},
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

