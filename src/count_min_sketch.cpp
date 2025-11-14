#include <cstdint>
#include <random>
#include <cassert>
#include <cmath>
#include <vector>
#include <cstring>
#include <cassert>
#include "count_min_sketch.hpp"

CountMinSketch::CountMinSketch() : m_width(0), m_depth(0), m_table{nullptr}, m_hash_coefficients{nullptr} {}

CountMinSketch::CountMinSketch(double error,
                               double probability,
                               uint64_t seed) : m_width(static_cast<uint32_t>(std::ceil(2 / error))),
                                                m_depth(static_cast<uint32_t>(std::ceil(std::log(1 / (1 - probability)) / std::log(2)))),
                                                m_table(new uint32_t*[m_depth]),
                                                m_hash_coefficients(new uint32_t[m_depth]) {
    assert(error > 0 && error < 1);
    assert(probability > 0 && probability < 1);

    for (uint32_t idx = 0; idx < m_depth; ++idx)
    {
        m_table[idx] = new uint32_t[m_width]();
    }

    std::mt19937 gen(static_cast<std::mt19937::result_type>(seed));
    std::uniform_int_distribution<uint32_t> dis(1, std::numeric_limits<uint32_t>::max() - 1);
    for (uint32_t idx = 0; idx < m_depth; ++idx)
    {
        m_hash_coefficients[idx] = dis(gen);
    }
}

CountMinSketch::CountMinSketch(const CountMinSketch& other) : m_width{other.m_width}, m_depth{other.m_depth}, m_table{nullptr}, m_hash_coefficients{nullptr}
{
    assert (this != &other);
    if (m_table != nullptr)
    {
        this->delete_table();
    }
    copy_table(other);
}

CountMinSketch& CountMinSketch::operator=(const CountMinSketch& other)
{
    assert (this != &other);
    assert (this->m_table == nullptr || (this->m_width == other.m_width && this->m_depth == other.m_depth));
    if (m_table != nullptr)
    {
        this->delete_table();
    }
    else
    {
        m_width = other.m_width;
        m_depth = other.m_depth;
    }
    copy_table(other);

    return *this;
}

CountMinSketch::~CountMinSketch() 
{
    if (m_table != nullptr)
    {
        this->delete_table();
    }
}

void CountMinSketch::delete_table()
{
    for (uint32_t idx = 0; idx < m_depth; ++idx)
    {
        delete[] m_table[idx];
    }
    delete[] m_table;
    delete[] m_hash_coefficients;
    m_table = nullptr;
    m_hash_coefficients = nullptr;
}

void CountMinSketch::copy_table(const CountMinSketch& other)
{
    assert(this->m_width == other.m_width && this->m_depth == other.m_depth);
    assert(m_table == nullptr && m_hash_coefficients == nullptr);
    m_table = new uint32_t*[m_depth];

    for (uint32_t idx = 0; idx < m_depth; ++idx)
    {
        m_table[idx] = new uint32_t[m_width]();
        std::memcpy(m_table[idx], other.m_table[idx], sizeof(uint32_t) * m_width);
    }

    m_hash_coefficients = new uint32_t[m_depth];
    std::memcpy(m_hash_coefficients, other.m_hash_coefficients, sizeof(uint32_t) * m_depth);
}

uint32_t CountMinSketch::hash(uint64_t key, uint32_t row) const
{
    uint64_t hash = (static_cast<uint64_t>(m_hash_coefficients[row]) * key);
    hash += (hash >> 32);
    hash &= PRIME;

    return hash;
}

void CountMinSketch::add(uint64_t item) {
    std::vector<uint32_t> hashes = std::vector<uint32_t>(m_depth);
    for (uint32_t row = 0; row < m_depth; ++row) {
        uint64_t hash_val = hash(item, row);
        hashes.push_back(static_cast<uint32_t>(hash_val));
    }

    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    for (uint32_t row = 0; row < m_depth; ++row) {
        uint32_t col = static_cast<uint32_t>(hashes[row] % m_width);
        if (m_table[row][col] < min_count) {
            min_count = m_table[row][col];
        }
    }

    for (uint32_t row = 0; row < m_depth; ++row) {
        uint32_t col = static_cast<uint32_t>(hashes[row] % m_width);
        if (m_table[row][col] == min_count) {
            ++m_table[row][col];
        }
    }
}

uint32_t CountMinSketch::estimate(uint64_t key) const
{
    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    for (uint32_t row = 0; row < m_depth; ++row) {
        uint64_t col = hash(key, row);
        if (uint32_t curr_count = m_table[row][col]; curr_count < min_count)
        {
            min_count = curr_count;
        }
    }

    return min_count;
}

void CountMinSketch::reduce()
{
    for (uint32_t row = 0; row < m_depth; ++row)
    {
        for (uint32_t col = 0; col < m_width; ++col)
        {
            m_table[row][col] <<= 1;
        }
    }
}
