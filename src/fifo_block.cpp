#include <cassert>
#include <string>
#include <cstdint>
#include <iostream>

#include "pipeline_block.hpp"


class FIFOBlock : public BasePipelineBlock {
public:
    explicit FIFOBlock(uint64_t capacity, uint64_t quantum_size, uint64_t quanta_allocation) 
            : BasePipelineBlock(capacity, quantum_size, quanta_allocation, "FIFO") {}
            
            FIFOBlock(const FIFOBlock& other) : BasePipelineBlock(other)
            {}

    FIFOBlock& operator=(const FIFOBlock& other)
    {
        if (this != &other)
        {
            BasePipelineBlock::operator=(other);
        }

        return *this;
    }

    QuantumMoveResult move_quanta_to(PipelineBlock& other) override {
        assert(m_cache_max_capacity >= m_quantum_size);
        m_arr.rotate();

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

        EntryData evicted_item = m_arr.pop_head();
        m_arr.push_tail(item);
        return std::make_pair(m_arr.size() - 1, evicted_item);
    }
};