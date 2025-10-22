#pragma once
#include <cstdint>
#include "utils.cpp"

namespace Constants {
    constexpr uint64_t PIPELINE_CACHE_CAPACITY = 256;
    constexpr uint64_t NUM_OF_QUONTA = 16;
    constexpr uint64_t QUANTUM_SIZE = PIPELINE_CACHE_CAPACITY / NUM_OF_QUONTA;
    constexpr uint64_t NUM_OF_BLOCKS = 3;
    constexpr uint64_t SAMPLE_RATE = 4;
    static_assert(utils::isPowerOfTwo(SAMPLE_RATE));
    constexpr uint64_t SAMPLE_MASK = SAMPLE_RATE - 1;
    constexpr uint64_t SAMPLED_CACHE_CAPACITY = PIPELINE_CACHE_CAPACITY / SAMPLE_RATE;
    constexpr uint64_t SAMPLED_QUANTUM_SIZE = QUANTUM_SIZE / SAMPLE_RATE;
    constexpr uint64_t AGING_WINDOW_SIZE = 10 * PIPELINE_CACHE_CAPACITY;
    constexpr uint64_t NUM_OF_ITEM_TO_SAMPLE = 16;
    constexpr double SKETCH_ERROR = 0.01;
    constexpr double SKETCH_PROB = 0.99;
    constexpr uint64_t AGING_MULTIPLIER = 10;
    constexpr uint64_t DECISION_WINDOW_SIZE = 8 * PIPELINE_CACHE_CAPACITY;
    constexpr uint64_t RANDOM_SEED = 42;
}