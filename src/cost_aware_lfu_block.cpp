#include <cassert>
#include <string>
#include <cstdint>
#include <random>
#include <algorithm>
#include <iostream>

#include "constants.hpp"
#include "pipeline_block.hpp"
#include "count_min_sketch.hpp"

static uint64_t get_score(const EntryData& entry, const CountMinSketch& sketch) {
    uint64_t freq = sketch.estimate(entry.id);
    return entry.latency * entry.tokens * freq;
}

class CostAwareLFUBlock : public BasePipelineBlock {
private:
    const CountMinSketch& m_sketch;
    std::mt19937 m_generator;

    public:
    explicit CostAwareLFUBlock(uint64_t capacity, uint64_t quantum_size, uint64_t quanta_allocation, const CountMinSketch& sketch)
            : BasePipelineBlock(capacity, quantum_size, quanta_allocation, "CostAwareLFU"),
              m_sketch(sketch),
              m_generator(Constants::RANDOM_SEED)
              {}

    CostAwareLFUBlock& operator=(const CostAwareLFUBlock& other)
    {
        if (this != &other)
        {
            BasePipelineBlock::operator=(other);
        }

        return *this;
    }

    CostAwareLFUBlock(const CostAwareLFUBlock& other) : BasePipelineBlock(other), 
                                                        m_sketch(other.m_sketch), 
                                                        m_generator(other.m_generator) {}

    QuantumMoveResult move_quanta_to(PipelineBlock& other) override {
        assert(m_arr.size() == m_curr_max_capacity || m_arr.empty());
        assert(m_curr_max_capacity >= m_quantum_size);
        assert(!other.get_arr().is_rotated());

        m_arr.rotate();
        EntryData* data = m_arr.data();
        std::nth_element(data, data + (m_arr.size() - m_quantum_size), data + m_arr.size(),
            [this](const EntryData& a, const EntryData& b) {
                return get_score(a, m_sketch) < get_score(b, m_sketch);
            });

        QuantumMoveResult result;
        result.items_moved = other.accept_quanta(m_arr);

        const uint64_t remaining_count = m_arr.size();
        for (uint64_t i = 0; i < remaining_count; ++i) {
            result.items_remaining.emplace_back(m_arr[i].id, i);
        }

        m_curr_max_capacity -= m_quantum_size;

        return result;
    }

    std::pair<uint64_t, std::optional<EntryData>> insert_item(EntryData item) override {
        if (m_arr.size() < m_curr_max_capacity) {
            assert(!m_arr.is_rotated());
            m_arr.push_tail(item);
            return std::make_pair(m_arr.size() - 1, std::nullopt);
        }

        std::uniform_int_distribution<uint64_t> distribution(0, m_arr.size() - 1);

        const uint64_t start_idx = distribution(m_generator);
        auto itr = m_arr.partial_iterator(start_idx);
        uint64_t idx_to_remove = start_idx;
        uint64_t lowest_score = get_score(m_arr[start_idx], m_sketch);

        for (uint64_t i = 0; i < Constants::NUM_OF_ITEM_TO_SAMPLE; ++i) {
            const EntryData& entry = *itr;
            ++itr;
            if (uint64_t curr_score = get_score(entry, m_sketch); curr_score < lowest_score) {
                lowest_score = curr_score;
                idx_to_remove = start_idx + i < m_arr.size() ? start_idx + i : (start_idx + i) - m_arr.size();
            }
        }

        if (lowest_score < get_score(item, m_sketch)) {
            return std::make_pair(std::numeric_limits<uint64_t>::max(), item);
        }

        EntryData evicted_item = m_arr.replace(idx_to_remove, item);
        return std::make_pair(idx_to_remove, evicted_item);
    }
};