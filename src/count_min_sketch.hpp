#pragma once
#include <cstdint>

class CountMinSketch {
private:
    constexpr static uint64_t PRIME = 1l << (61 - 1); // A large prime number for hashing

    uint32_t m_width;
    uint32_t m_depth;
    uint32_t** m_table;
    uint32_t* m_hash_coefficients;
    void delete_table();
    void copy_table(const CountMinSketch& other);
    [[nodiscard]] uint32_t hash(uint64_t key, uint32_t row) const;

public:
    CountMinSketch();
    CountMinSketch(double error, double probabilty, uint64_t seed);
    CountMinSketch(const CountMinSketch& other);
    ~CountMinSketch();

    CountMinSketch& operator=(const CountMinSketch& other);

    void add(uint64_t item);
    [[nodiscard]] uint32_t estimate(uint64_t item) const;
    void reduce();
};