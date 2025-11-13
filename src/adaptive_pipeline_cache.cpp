#include <array>
#include <vector>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include "utils.cpp"
#include "pipeline_block.hpp"
#include "xxhash.h"
#include "pipeline_cache.hpp"

#include <cassert>

using Json = nlohmann::json;

inline bool should_sample(uint64_t key, uint64_t seed, uint64_t sample_mask) {
    uint64_t hash = XXH3_64bits_withSeed(&key, sizeof(key), seed);
    return (hash & sample_mask) == 0;
}

class AdaptivePipelineCache {
private:
    PipelineCache m_main_cache;
    PipelineCacheProxy m_main_sampled;
    std::vector<PipelineCacheProxy> m_ghost_caches;
    uint64_t ops_since_last_decision;

    std::vector<std::pair<uint64_t, uint64_t>> m_ghost_caches_indeces;
    std::vector<std::string> m_ghost_caches_names;
    uint64_t m_num_of_ghost_caches;
    uint64_t m_seed;
    uint64_t m_decision_window_size;
    uint64_t m_sample_mask;

    void populate_ghost_indeces_and_names(uint64_t num_of_blocks, const std::vector<std::string>& cache_types)
    {
        m_num_of_ghost_caches = num_of_blocks * (num_of_blocks - 1);
        for (uint64_t i = 0; i < num_of_blocks; ++i)
        {
            for (uint64_t j = 0; j < num_of_blocks; ++j)
            {
                if (i != j)
                {
                    m_ghost_caches_indeces.push_back(std::make_pair(i, j));
                    const std::string cache_name = "-" + cache_types[i] + "+" + cache_types[j];
                    m_ghost_caches_names.push_back(cache_name);
                }
            }
        }
    }

public:
    explicit AdaptivePipelineCache(std::string config_path) : m_main_cache{config_path},
                                                              m_main_sampled{config_path},
                                                              m_ghost_caches{},
                                                              ops_since_last_decision{0}
    {
        std::ifstream config_file(config_path);
        if (config_file.is_open())
        {
            try {
                Json config = Json::parse(config_file);

                const uint64_t capacity = config["cache"]["capacity"].get<uint64_t>();

                const uint64_t num_of_blocks = config["cache"]["num_of_blocks"].get<uint64_t>();
                if (num_of_blocks <= 1)
                {
                    std::cerr << "num_of_blocks must be 2 or higher" << std::endl;
                    exit(1);
                }

                if (config["blocks"].size() != num_of_blocks)
                {
                    std::cerr << "mismatch between the number of blocks and their definitions" << std::endl;
                    exit(1);
                }

                std::vector<std::string> cache_types;
                uint64_t total_quanta = 0;
                for (const auto& block : config["blocks"])
                {
                    cache_types.push_back(block["type"].get<std::string>());
                    total_quanta += block["initial_quanta"].get<uint64_t>();
                }

                const uint64_t num_of_quanta = config["cache"]["num_of_quanta"].get<uint64_t>();

                if (total_quanta != num_of_quanta)
                {
                    std::cerr << "the total quanta isn't the same as the num_of_quanta" << std::endl;
                    exit(1);
                }

                populate_ghost_indeces_and_names(num_of_blocks, cache_types);

                m_seed = config["cache"]["seed"].get<uint64_t>();

                m_decision_window_size = capacity * config["cache"]["decision_window_multiplier"].get<uint64_t>();

                const uint64_t sample_rate = config["cache"]["sample_rate"].get<uint64_t>();

                if (!utils::is_power_of_two(sample_rate))
                {
                    std::cerr << "the sample_rate must be a power of two" << std::endl;
                    exit(1);
                }
                m_sample_mask = sample_rate - 1;
            }
            catch (const Json::exception& e) {
                std::cerr << "ERROR: Failed to read cache config: " << e.what() << "\n";
                exit(1);
            }

            m_ghost_caches.resize(m_num_of_ghost_caches);

            create_ghost_caches();

            config_file.close();
        } else {
            std::cerr << "Warning: config.json not found" << std::endl;
            exit(1);
        }
    }

    std::tuple<double, uint64_t> getitem(uint64_t key) 
    {
        ++ops_since_last_decision;
        const EntryData& entry = m_main_cache.get_item(key);
        std::tuple<double, uint64_t> item = std::make_tuple(entry.latency, entry.tokens);
        
        if (should_sample(key, m_seed, m_sample_mask))
        {   
            const double latency = entry.latency;
            const uint64_t tokens = entry.tokens;

            perform_op_on_ghost(m_main_sampled, key, latency, tokens);

            for (uint64_t type = 0; type < m_num_of_ghost_caches; ++type)
            {
                perform_op_on_ghost(m_ghost_caches[type], key, latency, tokens);
            }
        }

        return item;
    }

    void setitem(uint64_t key, const std::tuple<double, uint64_t>& value) 
    {
        ++ops_since_last_decision;
        const auto [latency, tokens] = value;
        m_main_cache.insert_item(key, latency, tokens);
        
        if (should_sample(key, m_seed, m_sample_mask))
        {   
            perform_op_on_ghost(m_main_sampled, key, latency, tokens);

            for (uint64_t type = 0; type < m_num_of_ghost_caches; ++type)
            {
                perform_op_on_ghost(m_ghost_caches[type], key, latency, tokens);
            }
        }

        if (ops_since_last_decision >= m_decision_window_size
            && m_main_cache.size() == m_main_cache.capacity())
        {
            ops_since_last_decision = 0;
            adapt();
        }
    }

    static void perform_op_on_ghost(PipelineCacheProxy& proxy, uint64_t key, double latency, uint64_t tokens)
    {
        if (proxy.contains(key))
        {
            proxy.get_item(key);
        }
        else 
        {
            proxy.insert_item(key, latency, tokens);
            if (proxy.should_evict())
            {
                proxy.evict_item();
            }
        }
    }

    void adapt()
    {
        ops_since_last_decision = 0;
        const double current_timeframe_cost = m_main_cache.get_timeframe_aggregated_cost();
        m_main_cache.reset_timeframe_stats();

        double minimal_timeframe_ghost_cost = std::numeric_limits<double>::max();
        uint64_t minimal_idx = std::numeric_limits<uint64_t>::max();

        for (uint64_t type = 0; type < m_num_of_ghost_caches; ++type)
        {
            const double curr_ghost_cache_cost = m_ghost_caches[type].get_timeframe_aggregated_cost();
            m_ghost_caches[type].reset_timeframe_stats();
            if (curr_ghost_cache_cost < minimal_timeframe_ghost_cost)
            {
                minimal_timeframe_ghost_cost = curr_ghost_cache_cost;
                minimal_idx = type;
            }
        }

        assert(minimal_idx < m_num_of_ghost_caches
            && minimal_timeframe_ghost_cost < std::numeric_limits<double>::max());

        if (minimal_timeframe_ghost_cost < current_timeframe_cost)
        {
            const std::pair<uint64_t, uint64_t> indeces_for_adaption = m_ghost_caches_indeces[minimal_idx];
            assert(m_main_cache.can_adapt(indeces_for_adaption.first, false) && m_main_cache.can_adapt(indeces_for_adaption.second, true));
            m_main_cache.move_quantum(indeces_for_adaption.first, indeces_for_adaption.second);
            m_main_sampled.move_quantum(indeces_for_adaption.first, indeces_for_adaption.second);

            create_ghost_caches();
        }

    }

    void create_ghost_caches()
    {
        m_main_sampled.prepare_for_copy();

        for (uint64_t type = 0; type < m_num_of_ghost_caches; ++type)
        {
            const std::pair<uint64_t, uint64_t> indeces = m_ghost_caches_indeces[type];
            m_ghost_caches[type] = m_main_sampled;
            if (m_main_sampled.can_adapt(indeces.first, false) && m_main_sampled.can_adapt(indeces.second, true))
            {
                m_ghost_caches[type].make_non_dummy();
                m_ghost_caches[type].move_quantum(indeces.first, indeces.second);
            }
            else
            {
                m_ghost_caches[type].make_dummy();
            }
        }
    }

    void delitem(uint64_t key) 
    {
        
    }

    bool contains(uint64_t key) const 
    {
        return m_main_cache.contains(key);
    }

    std::pair<uint64_t, std::tuple<double, uint64_t>> popitem() 
    {
        if (m_main_cache.should_evict())
        {
            const EntryData entry = m_main_cache.evict_item();

            return std::make_pair(entry.id, std::make_tuple(entry.latency, entry.tokens));
        }

        return std::make_pair(0, std::make_tuple(0, std::numeric_limits<uint64_t>::max()));
    }

    std::tuple<double, uint64_t> get(uint64_t key, const std::tuple<double, uint64_t>& default_value = std::make_tuple(0.0, 0)) 
    {
        return getitem(key);
    }

    std::vector<uint64_t> keys() const 
    {
        return m_main_cache.keys();
    }

    std::vector<std::tuple<double, uint64_t>> values() const 
    {
        return m_main_cache.values();
    }

    size_t maxsize() const { return m_main_cache.capacity(); }
    size_t currsize() const { return m_main_cache.size(); }
    bool empty() const { return m_main_cache.empty(); }

    void clear() 
    {
        m_main_cache.clear();
        m_main_sampled.clear();
        for (uint64_t type = 0; type < m_num_of_ghost_caches; ++type)
        {
            m_ghost_caches[type].clear();
        }
    }

    std::string repr() const 
    {
        return m_main_cache.get_current_config();
    }
};
