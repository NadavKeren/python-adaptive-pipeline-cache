#include <cassert>
#include <string>
#include <cstdint>
#include <random>
#include <algorithm>
#include <cassert>

#include "pipeline_block.hpp"

class ALRUBlock : public BasePipelineBlock {
private:
    std::mt19937 m_generator;
    const uint64_t m_sample_size;

public:
    explicit ALRUBlock(uint64_t capacity,
                       uint64_t quantum_size,
                       uint64_t quanta_allocation,
                       uint64_t seed,
                       uint64_t sample_size)
            : BasePipelineBlock{capacity, quantum_size, quanta_allocation, "ALRU"},
              m_generator{static_cast<std::mt19937::result_type>(seed)},
              m_sample_size(sample_size)
              {}

    ALRUBlock(const ALRUBlock& other) = default;

    ALRUBlock& operator=(const ALRUBlock& other)
    {
        if (this != &other)
        {
            BasePipelineBlock::operator=(other);
        }

        return *this;
    }

    QuantumMoveResult move_quanta_to(PipelineBlock& other) override {
        assert(m_arr.size() == m_curr_max_capacity || m_arr.empty());
        assert(m_curr_max_capacity >= m_quantum_size);

        QuantumMoveResult result;
        result.items_moved = other.accept_quanta(m_arr);

        const uint64_t remaining_count = m_arr.size();
        for (uint64_t i = 0; i < remaining_count; ++i) {
            result.items_remaining.emplace_back(m_arr[i].id, i);
        }

        m_curr_max_capacity -= m_quantum_size;

        return result;
    }

    InsertionResult insert_item(const EntryData& item) override {
        if (m_arr.size() < m_curr_max_capacity) {
            m_arr.push_tail(item);
            return InsertionResult{.was_item_inserted = true,
                                   .replaced_idx = m_arr.size() - 1,
                                   .removed_entry = std::nullopt};
        }

        std::uniform_int_distribution<uint64_t> distribution(0, m_arr.size() - 1);

        uint64_t start_idx = distribution(m_generator);
        auto itr = m_arr.partial_iterator(start_idx);
        uint64_t idx_to_remove = start_idx;
        uint64_t oldest_timestamp = m_arr[start_idx].last_access_time;

        for (uint64_t i = 0; i < std::min(m_sample_size, m_curr_max_capacity); ++i) {
            EntryData entry = *itr;
            ++itr;
            if (entry.last_access_time < oldest_timestamp) {
                oldest_timestamp = entry.last_access_time;
                idx_to_remove = start_idx + i < m_arr.size() ? start_idx + i : (start_idx + i) - m_arr.size();
            }
        }

        // Holding the latest timestamp directly allows us to reject items from the ALRU.
        if (oldest_timestamp > item.last_access_time) {
            return InsertionResult{.was_item_inserted = false,
                                   .replaced_idx = std::numeric_limits<uint64_t>::max(),
                                   .removed_entry = std::nullopt};
        }

        EntryData evicted_item = m_arr.replace(idx_to_remove, item);
        return InsertionResult{.was_item_inserted = true,
                               .replaced_idx = idx_to_remove,
                               .removed_entry = evicted_item};
    }

    void prepare_for_copy() override
    {
        m_arr.rotate();

        EntryData* data = m_arr.data();
        std::nth_element(data, data + (m_arr.size() - m_quantum_size), data + m_arr.size(),
            [](const EntryData& a, const EntryData& b) {
                return a.last_access_time < b.last_access_time;
            });
    }
};
