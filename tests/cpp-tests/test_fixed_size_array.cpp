#include <cstdint>
#include <iostream>

#include <gtest/gtest.h>
#include "fixed_size_array.hpp"

TEST(FixedSizeArrayTest, TestSimpleAddition) {
    FixedSizeArray<uint64_t> arr{5};
    EXPECT_EQ(arr.capacity(), 5);
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.add(i);
    }
    EXPECT_EQ(arr.size(), 5);

    for (uint64_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i);
    }

    uint64_t val = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it) {
        EXPECT_EQ(*it, val++);
    }

    val = 0;
    for (uint64_t it : arr) {
        EXPECT_EQ(it, val++);
    }
}

TEST(FixedSizeArrayTest, TestFillAndEmpty) {
    FixedSizeArray<uint64_t> arr{5};
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.push_tail(i);
    }

    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        uint64_t val = arr.pop_head();
        EXPECT_EQ(val, i);
        EXPECT_EQ(arr.size(), arr.capacity() - i - 1);
    }
    EXPECT_TRUE(arr.empty());
}


TEST(FixedSizeArrayTest, TestPopAndPush) {
    FixedSizeArray<uint64_t> arr{5};
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.push_tail(i);
    }

    uint64_t val = arr.pop_head();
    EXPECT_EQ(val, 0);
    for (uint64_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i + 1);
    }
    EXPECT_EQ(arr.size(), 4);

    arr.push_tail(5);
    EXPECT_EQ(arr.size(), 5);
    for (uint64_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(arr[i], i + 1);
    }
}

struct Temp {
    uint64_t num1;
    uint32_t num2;
    bool booly;

    bool operator==(const Temp& other) const {
        return (num1 == other.num1 && num2 == other.num2 && booly == other.booly);
    }

    bool operator!=(const Temp& other) const {
        return !(*this == other);
    }
};

TEST(FixedSizeArrayTest, PartialCopyToEmptyTest) {
    constexpr uint64_t arr_size = 5;
    constexpr uint64_t num_of_items_to_move = 3;
    FixedSizeArray<Temp> src{arr_size};
    FixedSizeArray<Temp> dest{arr_size};

    Temp item1{.num1 = 1,
               .num2 = 2,
               .booly = false};
    Temp item2{.num1 = 2,
               .num2 = 3,
               .booly = true};
    Temp item3{.num1 = 3,
               .num2 = 4,
               .booly = false};
    Temp item4{.num1 = 4,
               .num2 = 5,
               .booly = true};
    Temp item5{.num1 = 5,
               .num2 = 6,
               .booly = false};
    
    
    src.push_tail(item1);
    src.push_tail(item2);
    src.push_tail(item3);
    src.push_tail(item4);
    src.push_tail(item5);

    src.partial_move_to(dest, num_of_items_to_move);
    
    EXPECT_EQ(dest.size(), num_of_items_to_move);
    EXPECT_EQ(dest[0], item3);
    EXPECT_EQ(dest[1], item4);
    EXPECT_EQ(dest[2], item5);
}

TEST(FixedSizeArrayTest, PartialCopyToNonEmptyTest) {
    constexpr uint64_t arr_size = 5;
    constexpr uint64_t num_of_items_to_move = 3;
    FixedSizeArray<Temp> src{arr_size};
    FixedSizeArray<Temp> dest{arr_size};

    Temp item1{.num1 = 1,
               .num2 = 2,
               .booly = false};
    Temp item2{.num1 = 2,
               .num2 = 3,
               .booly = true};
    Temp item3{.num1 = 3,
               .num2 = 4,
               .booly = false};
    Temp item4{.num1 = 4,
               .num2 = 5,
               .booly = true};
    Temp item5{.num1 = 5,
               .num2 = 6,
               .booly = false};

    Temp item6{.num1 = 6,
               .num2 = 7,
               .booly = true};
    
    
    src.push_tail(item1);
    src.push_tail(item2);
    src.push_tail(item3);
    src.push_tail(item4);
    src.push_tail(item5);

    dest.push_tail(item6);
    const uint64_t original_size = dest.size();

    src.partial_move_to(dest, num_of_items_to_move);
    
    EXPECT_EQ(dest.size(), num_of_items_to_move + original_size);
    EXPECT_EQ(dest[0], item6);
    EXPECT_EQ(dest[1], item3);
    EXPECT_EQ(dest[2], item4);
    EXPECT_EQ(dest[3], item5);
}


TEST(FixedSizeArrayTest, RotateBackToFrontTest) {
    FixedSizeArray<uint64_t> arr{10};
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.push_tail(i);
    }

    for (uint64_t i = 0; i < 5; ++i) {
        arr.pop_head();
    }
    
    const uint64_t* data_before_rotate = arr.data();
    for (uint64_t i = 5; i < arr.size(); ++i) {
        EXPECT_EQ(data_before_rotate[i], i);
    }
    arr.rotate();
    const uint64_t* data_after_rotate = arr.data();
    for (uint64_t i = 0; i < arr.size(); ++i)
    {
        EXPECT_EQ(data_after_rotate[i], i + 5);
    }
}

TEST(FixedSizeArrayTest, SimpleRotateTest) {
    FixedSizeArray<uint64_t> arr{10};
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.push_tail(i);
    }

    for (uint64_t i = 0; i < 5; ++i) {
        arr.pop_head();
    }
    for (uint64_t i = 10; i < 15; ++i) {
        arr.push_tail(i);
    }
    
    const uint64_t* data_before_rotate = arr.data();
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        if (i < 5) {
            EXPECT_EQ(data_before_rotate[i], i + 10);
        } else {
            EXPECT_EQ(data_before_rotate[i], i);
        }
    }

    arr.rotate();
    const uint64_t* data_after_rotate = arr.data();
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        EXPECT_EQ(data_after_rotate[i], i + 5);
    }
}

TEST(FixedSizeArrayTest, ComplexRotateTest) {
    FixedSizeArray<uint64_t> arr{10};
    for (uint64_t i = 0; i < arr.capacity(); ++i) {
        arr.push_tail(i);
    }

    for (uint64_t i = 0; i < 3; ++i) {
        arr.pop_head();
    }

    const uint64_t* data_before_rotate = arr.data();
    for (uint64_t i = 3; i < arr.capacity(); ++i) {
        EXPECT_EQ(data_before_rotate[i], i);
    }

    arr.rotate();
    const uint64_t* data_after_rotate = arr.data();
    for (uint64_t i = 0; i < arr.size(); ++i) {
        EXPECT_EQ(data_after_rotate[i], i + 3);
    }
}


