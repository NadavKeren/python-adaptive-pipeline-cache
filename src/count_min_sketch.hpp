#pragma once
#include <cstdint>

class CountMinSketch {
private:
    constexpr static uint64_t PRIME = 1l << (61 - 1); // A large prime number for hashing

    const uint32_t width;
    const uint32_t depth;
    uint32_t** table;
    uint32_t* hash_coefficients;
    void delete_table();
    void copy_table(const CountMinSketch& other);
    uint32_t hash(uint64_t key, uint32_t row) const;

public:
    CountMinSketch(double error, double probabilty);
    CountMinSketch(const CountMinSketch& other);
    ~CountMinSketch();

    CountMinSketch& operator=(const CountMinSketch& other);

    void add(uint64_t item);
    uint32_t estimate(uint64_t item) const;
    void reduce();
};