#include <cassert>
#include <string>
#include <cstdint>

#include "pipeline_block.hpp"


class FIFOBlock : public BasePipelineBlock {
public:
    explicit FIFOBlock(uint64_t capacity, uint64_t quantum_size, uint64_t quanta_allocation) 
            : BasePipelineBlock(capacity, quantum_size, quanta_allocation, "FIFO") {}
            
            FIFOBlock(const FIFOBlock& other) = default;

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


    InsertionResult insert_item(const EntryData& item) override {
        if (m_arr.size() < m_curr_max_capacity) {
            assert(!m_arr.is_rotated());
            m_arr.push_tail(item);
            return InsertionResult{.was_item_inserted = true,
                                   .replaced_idx = m_arr.size() - 1,
                                   .removed_entry = std::nullopt};
        }

        EntryData evicted_item = m_arr.pop_head();
        m_arr.push_tail(item);
        return InsertionResult{.was_item_inserted = true,
                               .replaced_idx = m_arr.size() - 1,
                               .removed_entry = evicted_item};
    }

    void prepare_for_copy() override
    {
        this->m_arr.rotate();
    }
};