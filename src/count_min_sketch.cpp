#include <cstdint>
#include <random>
#include <cassert>
#include <cmath>
#include <vector>
#include <limits.h>
#include "count_min_sketch.hpp"
#include "constants.hpp"
#include <cstring>

CountMinSketch::CountMinSketch(double error, double probability) : width(static_cast<uint32_t>(std::ceil(2 / error))),
                                                                   depth(static_cast<uint32_t>(std::ceil(std::log(1 / (1 - probability)) / std::log(2)))),
                                                                   table(new uint32_t*[depth]),
                                                                   hash_coefficients(new uint32_t[depth]) {
    assert(error > 0 && error < 1);
    assert(probability > 0 && probability < 1);

    for (uint32_t idx = 0; idx < depth; ++idx) 
    {
        table[idx] = new uint32_t[width]();
    }

    std::mt19937 gen(Constants::RANDOM_SEED);
    std::uniform_int_distribution<uint32_t> dis(1, std::numeric_limits<uint32_t>::max() - 1);
    for (uint32_t idx = 0; idx < depth; ++idx)
    {
        hash_coefficients[idx] = dis(gen);
    }
}

CountMinSketch::CountMinSketch(const CountMinSketch& other) : width{other.width}, depth{other.depth}
{
    assert (this != &other);
    if (table != nullptr) 
    {
        this->delete_table();
    }
    copy_table(other);
}

CountMinSketch& CountMinSketch::operator=(const CountMinSketch& other)
{
    assert (this != &other);
    assert (this->width == other.width && this->depth == other.depth);
    if (table != nullptr) 
    {
        this->delete_table();
    }
    copy_table(other);

    return *this;
}

CountMinSketch::~CountMinSketch() 
{
    if (table != nullptr) 
    {
        this->delete_table();
    }
}

void CountMinSketch::delete_table()
{
    for (uint32_t idx = 0; idx < depth; ++idx) 
    {
        delete[] table[idx];
    }
    delete[] table;
    delete[] hash_coefficients;
    table = nullptr;
    hash_coefficients = nullptr;
}

void CountMinSketch::copy_table(const CountMinSketch& other)
{
    assert(this->width == other.width && this->depth == other.depth);
    assert(table == nullptr && hash_coefficients == nullptr);
    table = new uint32_t*[depth];

    for (uint32_t idx = 0; idx < depth; ++idx) 
    {
        table[idx] = new uint32_t[width]();
        std::memcpy(table[idx], other.table[idx], sizeof(uint32_t) * width);
    }

    hash_coefficients = new uint32_t[depth];
    std::memcpy(hash_coefficients, other.hash_coefficients, sizeof(uint32_t) * depth);
}

uint32_t CountMinSketch::hash(uint64_t key, uint32_t row) const
{
    uint64_t hash = (static_cast<uint64_t>(hash_coefficients[row]) * key);
    hash += (hash >> 32);
    hash &= PRIME;

    return hash;
}

void CountMinSketch::add(uint64_t item) {
    std::vector<uint32_t> hashes = std::vector<uint32_t>(depth);
    for (uint32_t row = 0; row < depth; ++row) {
        uint64_t hash_val = hash(item, row);
        hashes.push_back(static_cast<uint32_t>(hash_val));
    }

    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    for (uint32_t row = 0; row < depth; ++row) {
        uint32_t col = static_cast<uint32_t>(hashes[row] % width);
        if (table[row][col] < min_count) {
            min_count = table[row][col];
        }
    }

    for (uint32_t row = 0; row < depth; ++row) {
        uint32_t col = static_cast<uint32_t>(hashes[row] % width);
        if (table[row][col] == min_count) {
            ++table[row][col];
        }
    }
}

uint32_t CountMinSketch::estimate(uint64_t key) const
{
    uint32_t min_count = std::numeric_limits<uint32_t>::max();
    for (uint32_t row = 0; row < depth; ++row) {
        uint64_t col = hash(key, row);
        if (uint32_t curr_count = table[row][col]; curr_count < min_count)
        {
            min_count = curr_count;
        }
    }

    return min_count;
}

void CountMinSketch::reduce()
{
    for (uint32_t row = 0; row < depth; ++row) 
    {
        for (uint32_t col = 0; col < width; ++col)
        {
            table[row][col] <<= 1;
        }
    }
}