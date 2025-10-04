#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <array>
#include "constants.hpp"
#include "utils.cpp"
#include "fixed_size_array.hpp"

struct EntryData {
    uint64_t id;
    double latency;
    uint64_t tokens;
    uint64_t last_access_time;
    EntryData(uint64_t id, double latency, uint64_t tokens) : id(id), latency(latency), tokens(tokens), last_access_time(utils::get_current_time_in_ms()) {}
    EntryData() : id(0), latency(0.0), tokens(0), last_access_time(0) {}
};

using NewLocationData = std::array<std::pair<uint64_t, uint64_t>, Constants::QUANTUM_SIZE>;

class PipelineBlock {
public:
    virtual ~PipelineBlock() = default;
    // Using the Visitor pattern to allow direct memcpy.
    virtual NewLocationData move_quanta_to(PipelineBlock& other) = 0;
    virtual NewLocationData accept_quanta(FixedSizeArray<EntryData>& arr) = 0;

    virtual FixedSizeArray<EntryData>& get_arr() = 0;
    virtual std::pair<uint64_t, std::optional<EntryData>> insert_item(EntryData item) = 0;
    virtual uint64_t size() const = 0;
    virtual uint64_t capacity() const = 0;
    virtual bool is_full() const = 0;
    virtual EntryData* get_entry(uint64_t idx) = 0;
    virtual std::string get_type() const = 0;
    virtual void clear() = 0;
};

class BasePipelineBlock : public PipelineBlock 
{
protected:
    FixedSizeArray<EntryData> m_arr;
    const uint64_t m_cache_max_capacity;
    const uint64_t m_quantum_size;
    uint64_t m_curr_max_capacity;
    const std::string m_type;
public:
    BasePipelineBlock(uint64_t cache_capacity, 
                      uint64_t quantum_size, 
                      uint64_t curr_quanta_alloc, 
                      const std::string& type) : m_arr{cache_capacity},
                                                 m_cache_max_capacity {cache_capacity},
                                                 m_quantum_size{quantum_size},
                                                 m_curr_max_capacity{m_quantum_size * curr_quanta_alloc},
                                                 m_type {type}
                                                 {}
    
    BasePipelineBlock(const BasePipelineBlock& other) : m_arr{other.m_arr},
                                                        m_cache_max_capacity{other.m_curr_max_capacity},
                                                        m_quantum_size{other.m_quantum_size},
                                                        m_curr_max_capacity{other.m_curr_max_capacity},
                                                        m_type{other.m_type}
                                                  {}
    
    virtual ~BasePipelineBlock() = default;

    
    NewLocationData accept_quanta(FixedSizeArray<EntryData>& arr) 
    {
        assert(arr.size() + this->m_quantum_size <= arr.capacity());
        this->m_arr.rotate();
        arr.partial_move_to(m_arr, this->m_quantum_size);
        NewLocationData locations{};
        const uint64_t offset = arr.size() - this->m_quantum_size;
        for (uint64_t i = 0; i < this->m_quantum_size; ++i) {
            locations[i] = std::make_pair(arr[offset + i].id, offset + i);
        }

        this->m_curr_max_capacity += this->m_quantum_size;
        assert(this->m_curr_max_capacity <= this->m_cache_max_capacity);

        return locations;
    }

    FixedSizeArray<EntryData>& get_arr() override { return m_arr; };

    uint64_t size() const override { return m_arr.size(); };

    uint64_t capacity() const override { return m_curr_max_capacity; }

    bool is_full() const override { return size() == capacity();}

    EntryData* get_entry(uint64_t idx) override 
    {
        assert(idx >= 0 && idx < m_arr.size());
        return m_arr.get_item(idx);
    }

    std::string get_type() const override { return m_type; }

    void clear() override { m_arr.clear(); }
};