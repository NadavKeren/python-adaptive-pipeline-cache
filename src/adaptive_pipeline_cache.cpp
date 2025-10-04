#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/operators.h>
#include <array>
#include <vector>
#include <tuple>
#include <cstdint>
#include "xxhash.h"
#include "pipeline_cache.hpp"

namespace py = pybind11;

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
    explicit AdaptivePipelineCache(size_t maxsize) : m_main_cache{}, m_main_sampled{}, m_ghost_caches{}, ops_since_last_decision{0}
    {
        assert(maxsize == Constants::PIPELINE_CACHE_CAPACITY);
        
        for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
        {
            const auto& blks_to_transfer = ghost_caches_indeces[type];
            m_ghost_caches[type].move_quantum(blks_to_transfer.first, blks_to_transfer.second);
        }
    }

    std::tuple<double, uint64_t> getitem(uint64_t key) 
    {
        ++ops_since_last_decision;
        const auto& entry = m_main_cache.get_item(key);
        std::tuple<double, uint64_t> item = std::make_tuple(entry.latency, entry.tokens);
        
        if (should_sample(key))
        {
            m_main_sampled.get_item(key);
            for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
            {
                m_ghost_caches[type].get_item(key);
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
            m_main_sampled.insert_item(key, latency, tokens);
            if (m_main_sampled.should_evict())
            {
                m_main_sampled.evict_item();
            }

            for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
            {
                m_ghost_caches[type].insert_item(key, latency, tokens);
                if (m_ghost_caches[type].should_evict())
                {
                    m_ghost_caches[type].evict_item();
                }
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

            for (uint64_t type = 0; type < GhostCaches::NUM_GHOST_CACHES; ++type)
            {
                const std::pair<uint64_t, uint64_t> indeces = ghost_caches_indeces[type];
                m_ghost_caches[type] = m_main_sampled;
                if (m_main_sampled.can_adapt(indeces.first, indeces.second))
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

// IMPORTANT: The module name here MUST match the name in setup.py ext_modules
PYBIND11_MODULE(_adaptive_pipeline_cache_impl, m) {
    m.doc() = "Internal C++ implementation of The Pipeline Cache";

    py::class_<AdaptivePipelineCache>(m, "AdaptivePipelineCacheImpl")
        .def(py::init<size_t>(), "Initialize Pipeline cache with maximum size")
        .def("__getitem__", &AdaptivePipelineCache::getitem)
        .def("__setitem__", &AdaptivePipelineCache::setitem)
        .def("__delitem__", &AdaptivePipelineCache::delitem)
        .def("__contains__", &AdaptivePipelineCache::contains)
        .def("__len__", &AdaptivePipelineCache::currsize)
        .def("__repr__", &AdaptivePipelineCache::repr)
        .def("popitem", &AdaptivePipelineCache::popitem)
        .def("get", &AdaptivePipelineCache::get, py::arg("key"), py::arg("default") = std::make_tuple(0.0, 0))
        .def("keys", &AdaptivePipelineCache::keys)
        .def("values", &AdaptivePipelineCache::values)
        .def("clear", &AdaptivePipelineCache::clear)
        .def_property_readonly("maxsize", &AdaptivePipelineCache::maxsize)
        .def_property_readonly("currsize", &AdaptivePipelineCache::currsize)
        .def("empty", &AdaptivePipelineCache::empty);
}
