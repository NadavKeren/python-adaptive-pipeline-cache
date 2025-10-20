# Adaptive Pipeline Cache

A high-performance adaptive caching system implemented in C++ with Python bindings. The cache dynamically adjusts its internal structure based on workload characteristics, combining FIFO, Approximate LRU (ALRU), and Cost-Aware LFU strategies.

## Features

- **Adaptive Architecture**: Automatically adjusts cache structure based on workload patterns
- **Multiple Eviction Policies**: Combines FIFO, ALRU, and Cost-Aware LFU in a pipeline
- **High Performance**: Implemented in C++20 with efficient memory management
- **Python Integration**: Clean Python API via pybind11
- **Cost-Aware**: Takes into account both latency and token costs for eviction decisions

## Installation

```bash
pip install adaptive-pipeline
```

### Building from Source

Requirements:
- C++20 compatible compiler
- CMake >= 3.15
- Python >= 3.7
- pybind11 >= 2.6.0

```bash
git clone <repository-url>
cd pipeline-cache
pip install -e .
```

## Usage

Notice that for this version, the size of the cache is hard-coded to be 1024, and it ignores the size parameter.

```python
from adaptive_pipeline import AdaptivePipelineCache

# Create a cache with capacity of 1024 items
cache = AdaptivePipelineCache(1024)

# Store items with (latency, tokens) tuple
cache[key] = (latency, tokens)

# Check if key exists
if key in cache:
    latency, tokens = cache[key]

# Get cache statistics
print(f"Current size: {cache.currsize}")
print(f"Max size: {cache.maxsize}")
```

## How It Works

The Adaptive Pipeline Cache uses a novel approach that:

1. **Pipeline Structure**: Divides the cache into three blocks (FIFO, ALRU, Cost-Aware LFU)
2. **Dynamic Adaptation**: Periodically evaluates alternative configurations using "ghost caches"
3. **Quantum-Based Resizing**: Moves chunks of items between blocks based on performance metrics
4. **Cost-Aware Eviction**: Considers both access frequency and cost (latency × tokens) for eviction decisions

The cache automatically adapts its configuration every 10,240 operations (configurable) by comparing the performance of the current configuration against alternative configurations simulated by ghost caches.

## Interface Compatibility

The implementation follows the interface of `cacheutils` for compatibility with GPT-Cache experiments.

## License

MIT License - see LICENSE file for details

## Author

Nadav Keren (nadavker@pm.me)