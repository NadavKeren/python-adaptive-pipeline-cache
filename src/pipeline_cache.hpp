#pragma once
#include <cassert>
#include <cstdint>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>

#include "count_min_sketch.hpp"
#include "pipeline_block.hpp"


struct EntryPosition {
    uint64_t id;
    uint64_t block_num;
    uint64_t idx;
};

class IPipelineCache {
public:
    virtual ~IPipelineCache();
    virtual const EntryData& get_item(uint64_t key) = 0;
    virtual void insert_item(uint64_t key, double latency, uint64_t tokens) = 0;
    [[nodiscard]] virtual bool contains(uint64_t key) const = 0;
    virtual EntryData evict_item() = 0;
    [[nodiscard]] virtual bool should_evict() const = 0;
    virtual void move_quantum(uint64_t src_block, uint64_t dest_block) = 0;

    [[nodiscard]] virtual std::vector<uint64_t> keys() const = 0;
    [[nodiscard]] virtual std::vector<std::tuple<double, uint64_t>> values() const = 0;
    [[nodiscard]] virtual size_t capacity() const = 0;
    [[nodiscard]] virtual size_t size() const = 0;
    [[nodiscard]] virtual bool empty() const = 0;
    virtual void clear() = 0;
    [[nodiscard]] virtual bool can_adapt(uint64_t block_num, bool increase) const = 0;
    [[nodiscard]] virtual double get_timeframe_aggregated_cost() const = 0;
    virtual void reset_timeframe_stats() = 0;
    virtual void prepare_for_copy() = 0;
};

class PipelineCache : private IPipelineCache{
private:
    class TimeframeStats {
        public:
        uint64_t ops;
        double aggregated_cost;

        TimeframeStats() : ops(0), aggregated_cost(0.0) {};

        void reset() {
            ops = 0;
            aggregated_cost = 0.0;
        }

        [[nodiscard]] double get_average_cost() const {
            return ops > 0 ? aggregated_cost / static_cast<double>(ops) : std::numeric_limits<double>::max();
        }
    };

    void age_sketch_if_needed();

    uint64_t m_cache_capacity;
    uint64_t m_quantum_size;
    std::unordered_map<uint64_t, EntryPosition> m_items;
    std::vector<std::unique_ptr<PipelineBlock>> m_blocks;
    std::vector<uint64_t> m_quanta_alloc;
    std::vector<EntryData> m_eviction_queue;
    uint64_t m_num_of_quanta;
    CountMinSketch m_sketch;
    uint64_t m_aging_window_size = 0;
    uint64_t m_ops_since_last_aging = 0;
    TimeframeStats m_stats;

public:
    PipelineCache();
    explicit PipelineCache(const std::string& config_path);
    explicit PipelineCache(bool is_sampled, const std::string& config_path);
    PipelineCache(const PipelineCache& other);
    PipelineCache& operator=(const PipelineCache& other);

    const EntryData& get_item(uint64_t key) override;
    void insert_item(uint64_t key, double latency, uint64_t tokens) override;
    bool contains(uint64_t key) const override;
    EntryData evict_item() override;
    bool should_evict() const override;
    void move_quantum(uint64_t src_block, uint64_t dest_block) override;

    std::vector<uint64_t> keys() const override;
    std::vector<std::tuple<double, uint64_t>> values() const override;
    size_t capacity() const override;
    size_t size() const override;
    bool empty() const override;
    void clear() override;
    bool can_adapt(uint64_t block_num, bool increase) const override;
    double get_timeframe_aggregated_cost() const override;
    void reset_timeframe_stats() override;

    void prepare_for_copy() override;

    std::string get_current_config() const;

private:
    void validate_sizes() const;
};

class PipelineCacheProxy : private IPipelineCache {
private:
    PipelineCache m_cache;
    bool is_in_dummy_mode;
public:
    PipelineCacheProxy();
    explicit PipelineCacheProxy(const std::string& config_path);
    PipelineCacheProxy(const PipelineCacheProxy& other);
    PipelineCacheProxy& operator=(const PipelineCacheProxy& other);

    const EntryData& get_item(uint64_t key) override;
    void insert_item(uint64_t key, double latency, uint64_t tokens) override;
    bool contains(uint64_t key) const override;
    EntryData evict_item() override;
    bool should_evict() const override;
    void move_quantum(uint64_t src_block, uint64_t dest_block) override;
    std::vector<uint64_t> keys() const override;
    std::vector<std::tuple<double, uint64_t>> values() const override;
    size_t capacity() const override;
    size_t size() const override;
    bool empty() const override;
    void clear() override;
    bool can_adapt(uint64_t block_num, bool increase) const override;
    double get_timeframe_aggregated_cost() const override;
    void reset_timeframe_stats() override;
    void prepare_for_copy() override;
    void make_dummy();
    void make_non_dummy();
};
