#pragma once

#include <cstdint>
#include <cstring>
#include <libassert/assert.hpp>

template<typename T>
class FixedSizeArray {

private:
    T* m_data = nullptr;
    uint64_t m_capacity;
    uint64_t m_size;
    uint64_t m_head;
    uint64_t m_tail;

public:
    explicit FixedSizeArray(uint64_t capacity) : m_capacity(capacity), m_size(0), m_head(0), m_tail(0) 
    {
        m_data = new T[m_capacity];
    }

    FixedSizeArray(const FixedSizeArray& other) 
    {
        DEBUG_ASSERT(!other.is_rotated());
        if (m_data != nullptr) 
        {
            DEBUG_ASSERT(m_capacity == other.m_capacity);
        }
        else
        {
            m_capacity = other.m_capacity;
            m_data = new T[m_capacity];
        }

        m_size = other.m_size;
        m_head = other.m_head;
        m_tail = other.m_tail;
        std::memcpy(m_data, other.m_data, m_size * sizeof(T));
        DEBUG_ASSERT(m_tail == m_head + m_size);
    }

    FixedSizeArray& operator=(const FixedSizeArray& other) 
    {
        DEBUG_ASSERT(!other.is_rotated());
        if (this != &other) {
            if (m_data != nullptr) 
            {
                DEBUG_ASSERT(m_capacity == other.m_capacity);
            }
            else 
            {
                m_capacity = other.m_capacity;
                m_data = new T[m_capacity];
            }

            m_size = other.m_size;
            m_head = other.m_head;
            m_tail = other.m_tail;
            std::memcpy(m_data, other.m_data, m_size * sizeof(T));
            DEBUG_ASSERT(m_tail == m_head + m_size);
        }
        return *this;
    }

    FixedSizeArray(FixedSizeArray&& other) noexcept 
        : m_capacity(other.m_capacity), m_size(other.m_size) 
    {
        other.rotate();
        m_data = other.m_data;
        m_head = 0;
        m_tail = m_size;
        DEBUG_ASSERT(this->m_capacity == other.m_capacity);

        other.m_data = nullptr;
        other.m_capacity = -1;
        other.m_size = -1;
        other.m_head = -1;
        other.m_tail = -1;
    }

private:
    [[nodiscard]] uint64_t find_index(uint64_t index) const
    {
        index = index > m_size ? index - m_size : index;
        return (m_head + index) < m_capacity ? m_head + index : (m_head + index) - m_capacity;
    }

    [[nodiscard]] uint64_t calc_size() const
    {
        return m_tail > m_head || m_tail == m_head && m_size == 0 
               ? m_tail - m_head 
               : m_tail + (m_capacity - m_head);
    }

public:
    [[nodiscard]] bool is_rotated() const { return m_tail < m_head || m_head > 0; }

    ~FixedSizeArray() { delete[] m_data; }

    void push_tail(const T& value) 
    {
        DEBUG_ASSERT(!is_full());
        
        m_data[m_tail] = value;
        m_tail = (m_tail + 1) < m_capacity ? m_tail + 1 : 0;
        ++m_size;

        DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);
    }

    T pop_head() 
    {
        DEBUG_ASSERT(!empty());
        
        T value = m_data[m_head];
        m_head = (m_head + 1) < m_capacity ? m_head + 1 : 0;
        --m_size;

        DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);
        return value;
    }

    T& operator[](uint64_t index) 
    {
        DEBUG_ASSERT(index < m_capacity);
        return m_data[find_index(index)];
    }

    const T& operator[](uint64_t index) const 
    {
        DEBUG_ASSERT(index < m_capacity);
        return m_data[find_index(index)];
    }

    T* get_item(uint64_t index) 
    {
        DEBUG_ASSERT(index <= m_capacity);
        return &m_data[find_index(index)];
    }

    void add(const T& value) 
    {
        push_tail(value);
    }

    T replace(uint64_t index, const T& value) 
    {
        DEBUG_ASSERT(index < m_size);
        
        uint64_t real_index = find_index(index);
        T old_value = m_data[real_index];
        m_data[real_index] = value;

        DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);

        return old_value;
    }

    [[nodiscard]] uint64_t capacity() const
    {
        return m_capacity;
    }

    [[nodiscard]] uint64_t size() const
    {
        return m_size;
    }

    [[nodiscard]] bool is_full() const
    {
        return m_size == m_capacity;
    }

    [[nodiscard]] bool empty() const
    {
        return m_size == 0;
    }

    void partial_move_to(FixedSizeArray& other, uint64_t count) 
    {
        if (!this->empty())
        {
            DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);
            DEBUG_ASSERT(other.calc_size() == other.m_size, other.m_tail, other.m_head, other.m_size, other.m_capacity);
            DEBUG_ASSERT(count <= m_size || count + other.m_size > other.m_capacity);
            DEBUG_ASSERT(m_head == 0 && m_tail == m_head + m_size || this->is_full());
            
            std::memcpy(other.m_data + other.m_size, m_data, count * sizeof(T));
            other.m_size += count;
            other.m_tail += count;
            m_head += count;
            m_size -= count;
            this->rotate();
            DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);
            DEBUG_ASSERT(other.calc_size() == other.m_size, other.m_tail, other.m_head, other.m_size, other.m_capacity);
        }
    }

    void rotate()
    {
        if (!is_rotated()) {
            return;
        }

        if (m_head < m_tail) 
        {
            std::memcpy(m_data, m_data + m_head, m_size * sizeof(T));
        }
        else
        {
            T* temp = new T[m_capacity];
            const uint64_t items_to_move_to_beginning = m_capacity - m_head; 
            std::memcpy(temp, m_data + m_head, items_to_move_to_beginning * sizeof(T));
            std::memcpy(temp + items_to_move_to_beginning, m_data, m_tail * sizeof(T));

            delete[] m_data;
            m_data = temp;
        }

        m_head = 0;
        m_tail = m_size;
        DEBUG_ASSERT(calc_size() == m_size, m_tail, m_head, m_size, m_capacity);
    }

    T* data() { return m_data; }

    void clear()
    {
        m_head = 0;
        m_tail = 0;
        m_size = 0;
    }

    
    public:
    class iterator {
        FixedSizeArray* arr;
        uint64_t idx;
        public:
        explicit iterator(FixedSizeArray* arr) : arr(arr), idx(0) {}
        iterator(FixedSizeArray* arr, uint64_t idx) : arr(arr), idx(idx) {}
        iterator& operator++() { ++idx; return *this; }
        bool operator!=(const iterator& other) const { return idx != other.idx; }
        T& operator*() { return (*arr)[arr->find_index(idx)]; }
    }; // class iterator
    
    class const_iterator {
        const FixedSizeArray* arr;
        uint64_t idx;
        public:
        explicit const_iterator(const FixedSizeArray* arr) : arr(arr), idx(0) {}
        const_iterator(const FixedSizeArray* arr, uint64_t idx) : arr(arr), idx(arr->find_index(idx)) {}
        const_iterator& operator++() { ++idx; return *this; }
        bool operator!=(const const_iterator& other) const { return idx != other.idx; }
        const T& operator*() const { return (*arr)[arr->find_index(idx)]; }
        
    }; // class const_iterator
    
    iterator begin() { return iterator(this, this->m_head); }
    iterator end() { return iterator(this, this->m_tail); }
    const_iterator cbegin() const { return const_iterator(this, this->m_head); }
    const_iterator cend() const { return const_iterator(this, this->m_tail); }

    FixedSizeArray::iterator partial_iterator(uint64_t idx)
    {
        DEBUG_ASSERT(idx < m_size);
        return iterator(this, find_index(idx));
    }
}; // class FixedSizeArray
