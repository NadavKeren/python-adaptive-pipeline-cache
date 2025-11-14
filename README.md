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
cache = AdaptivePipelineCache("./config.json")

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


## Configuring the cache
The cache configuration can be changed by editing the config.json file, the default configuration is:

```json
{
  "cache": {
    "capacity": 1024,
    "num_of_quanta": 16,
    "num_of_blocks": 3,
    "sample_rate": 4,
    "decision_window_multiplier": 10,
    "aging_window_multiplier": 10,
    "seed" : 42,
    "sample_size" : 16
  },
  "count_min_sketch": {
    "error": 0.01,
    "probability": 0.99
  },
  "blocks": [
    {
      "type" : "fifo",
      "initial_quanta": 5
    },
    {
      "type" : "alru",
      "initial_quanta": 5
    },
    {
      "type" : "cost_aware_lfu",
      "initial_quanta": 6
    }
  ]
}
```
The most important options are:
> "capacity" : 1024 

Which is the total capacity (in items) of the cache,

> "num_of_quanta" : 16

The amount of quanta to which the cache is divided, 16 is a good middle-ground between granularity and fast adaptability.

> "sample_rate" : 4
 
This means that 1/4 of the items are sampled in order to determine what adaption is optimal. The lower the sampling is,
the less accurate it is, but the sampling overhead is also smaller.
Our experiments show that 1/4 to 1/8 is a decent balance between performance and accuracy.

**Important Remark**: You should verify that ```capacity / (num_of_quanta * sample_rate) > 1```, 
and best if it is greater or equal to 4.

> "sample_size"
 
Under the hood, our algorithm uses k-associativity to pick a cache victim. 
This option determines how many items are checked. Previous research showed that 16-32 is a good balance.

#### Configuring the blocks
Since the cache contains several different policies, each governing a different *block* of the cache,
you can configure what blocks appear in the pipeline, what are their initial quota, and *their ordering*.
The configuration file allows using the currently implemented blocks:

* First-In-First-Out - ```"fifo"```
* Approximate Least-Recently-Used - ```"alru"```
* Cost-Aware Least-Frequently-Used - ```"cost_aware_lfu"```

For each block, you must configure an initial quanta allocation, where the sum of quanta is ```"num_of_quanta"``` defined above.
For ***long enough workloads***, the initial starting point is not significant.

Additional block types can be suggested on the github page of the project.

## License

MIT License - see LICENSE file for details

## Author

Nadav Keren (nadavker@pm.me)