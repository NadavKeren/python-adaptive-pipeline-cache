#include <array>
#include <vector>
#include <tuple>
#include <cstdint>
#include <iostream>
#include <fstream>
#include "constants.hpp"
#include "pipeline_block.hpp"
#include "xxhash.h"
#include "pipeline_cache.hpp"

enum GhostCaches {
    FIFO_ALRU,
    FIFO_COST,
    ALRU_FIFO,
    ALRU_COST,
    COST_FIFO,
    COST_ALRU,
    NUM_GHOST_CACHES
};

constexpr std::array<std::pair<uint64_t, uint64_t>, (GhostCaches::NUM_GHOST_CACHES + 1)> ghost_caches_indeces
{
    std::make_pair(0, 1), 
    std::make_pair(0, 2),
    std::make_pair(1, 0),
    std::make_pair(1, 2),
    std::make_pair(2, 0),
    std::make_pair(2, 1),
    std::make_pair(std::numeric_limits<uint64_t>::max(), std::numeric_limits<uint64_t>::max())
};

inline bool should_sample(uint64_t key) {
    uint64_t hash = XXH3_64bits(&key, sizeof(key));
    return (hash & Constants::SAMPLE_MASK) == 0;
}

class AdaptivePipelineCache {
private:
    PipelineCache m_main_cache;
    PipelineCacheProxy m_main_sampled;
    std::array<PipelineCacheProxy, GhostCaches::NUM_GHOST_CACHES> m_ghost_caches;
    uint64_t ops_since_last_decision;

public:
    explicit AdaptivePipelineCache(size_t /*maxsize*/) : m_main_cache{}, m_main_sampled{}, m_ghost_caches{}, ops_since_last_decision{0}
    {
        create_ghost_caches();
    }

    std::tuple<double, uint64_t> getitem(uint64_t key) 
    {
        ++ops_since_last_decision;
        const EntryData& entry = m_main_cache.get_item(key);
        std::tuple<double, uint64_t> item = std::make_tuple(entry.latency, entry.tokens);
        
        if (should_sample(key))
        {   
            const double latency = entry.latency;
            const uint64_t tokens = entry.tokens;

            perform_op_on_ghost(m_main_sampled, key, latency, tokens);

            for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
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
        
        if (should_sample(key))
        {   
            perform_op_on_ghost(m_main_sampled, key, latency, tokens);

            for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
            {
                perform_op_on_ghost(m_ghost_caches[type], key, latency, tokens);
            }
        }

        if (ops_since_last_decision >= Constants::DECISION_WINDOW_SIZE && m_main_cache.size() == m_main_cache.capacity())
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

        for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
        {
            const double curr_ghost_cache_cost = m_ghost_caches[type].get_timeframe_aggregated_cost();
            m_ghost_caches[type].reset_timeframe_stats();
            if (curr_ghost_cache_cost < minimal_timeframe_ghost_cost)
            {
                minimal_timeframe_ghost_cost = curr_ghost_cache_cost;
                minimal_idx = type;
            }
        }

        assert(minimal_idx < GhostCaches::NUM_GHOST_CACHES
            && minimal_timeframe_ghost_cost < std::numeric_limits<double>::max());

        if (minimal_timeframe_ghost_cost < current_timeframe_cost)
        {
            const std::pair<uint64_t, uint64_t> indeces_for_adaption = ghost_caches_indeces[minimal_idx];
            assert(m_main_cache.can_adapt(indeces_for_adaption.first, indeces_for_adaption.second));
            m_main_cache.move_quantum(indeces_for_adaption.first, indeces_for_adaption.second);
            m_main_sampled.move_quantum(indeces_for_adaption.first, indeces_for_adaption.second);

            create_ghost_caches();
        }

    }

    void create_ghost_caches()
    {
        m_main_sampled.prepare_for_copy();

        for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
        {
            const std::pair<uint64_t, uint64_t> indeces = ghost_caches_indeces[type];
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
        assert(m_main_cache.should_evict());
        const EntryData entry = m_main_cache.evict_item();

        return std::make_pair(entry.id, std::make_tuple(entry.latency, entry.tokens));
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
        for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
        {
            m_ghost_caches[type].clear();
        }
    }

    std::string repr() const 
    {
        return m_main_cache.get_current_config();
    }
};
