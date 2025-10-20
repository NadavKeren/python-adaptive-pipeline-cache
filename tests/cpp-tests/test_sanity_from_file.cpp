#include <string>
#include <cstdint>
#include <cassert>
#include <iostream>
#include <fstream>
#include <sstream>

#include "adaptive_pipeline_cache.cpp"

int main() {
    const std::string input_file = "input.trace";

    AdaptivePipelineCache cache(1024);

    std::cout << "\n--- AdaptivePipelineCache Sanity Test Start ---\n";

    std::ifstream ifs(input_file);
    if (!ifs.is_open()) {
        std::cerr << "Error: Could not open log file " << input_file << " for reading.\n";
        return 1;
    }

    std::string line;
    uint64_t line_count = 0;
    uint64_t hits = 0;
    double cost = 0;
    double total_latency = 0;
    uint64_t total_tokens = 0;

    bool is_cache_full = false;
    while (std::getline(ifs, line)) {
        line_count++;
        std::stringstream ss(line);
        uint64_t timestamp, key, tokens;
        double latency;

        if (!(ss >> timestamp >> key >> latency >> tokens)) 
        {
            std::cerr << "Error parsing line " << line_count << ": '" << line << std::endl;
            exit(1);
        }
        
        is_cache_full = is_cache_full || cache.currsize() == cache.maxsize();

        if (cache.contains(key)) 
        {
            ++hits;
            std::tuple<double, uint64_t> result = cache.getitem(key);
        } 
        else 
        {
            total_latency += latency;
            total_tokens += tokens;
            cost += latency * tokens;
            std::tuple<double, uint64_t> value = std::make_tuple(latency, tokens);
            cache.setitem(key, value);
            
            if (is_cache_full)
            {
                cache.popitem();
            }
        }
    }

    ifs.close();
    
    std::cout << "Requests: " << line_count
              << "\tHit ratio: " << (static_cast<double>(hits) / line_count) * 100
              << "\tAvg. cost: " << cost / line_count 
              << "\tAvg. Latency: " << total_latency / line_count
              << "\tAvg. tokens: " << total_tokens / line_count << std::endl;

    std::cout << "--- AdaptivePipelineCache Sanity Test End ---\n";

    return 0;
}