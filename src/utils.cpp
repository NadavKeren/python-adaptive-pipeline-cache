#pragma once
#include <chrono>
#include <cstdint>

namespace utils {
    static uint64_t get_current_time_in_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    inline constexpr bool is_power_of_two(unsigned int n) 
    {
        return (n > 0) && ((n & (n - 1)) == 0);
    }
}