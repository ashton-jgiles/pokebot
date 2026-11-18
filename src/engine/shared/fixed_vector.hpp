#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace pokebot {

    // A list with a fixed maximum size, stored inline with no heap allocation.
    // Copying it is a plain memory copy, which keeps GameState cheap to copy for search.
    template <class T, std::size_t N>
    class FixedVector {
        static_assert(N <= UINT16_MAX, "FixedVector capacity must fit in 16 bits");

    public:
        using value_type = T;
        using iterator = T*;
        using const_iterator = const T*;

        FixedVector() = default;
        FixedVector(std::initializer_list<T> values) {
            for (const T& value : values) {
                push_back(value);
            }
        }

        static constexpr std::size_t capacity() { return N; }
        std::size_t size() const { return size_; }
        bool empty() const { return size_ == 0; }
        bool full() const { return size_ == N; }

        T& operator[](std::size_t i) { return items_[i]; }
        const T& operator[](std::size_t i) const { return items_[i]; }
        T& front() { return items_[0]; }
        const T& front() const { return items_[0]; }
        T& back() { return items_[size_ - 1]; }
        const T& back() const { return items_[size_ - 1]; }

        iterator begin() { return items_.data(); }
        iterator end() { return items_.data() + size_; }
        const_iterator begin() const { return items_.data(); }
        const_iterator end() const { return items_.data() + size_; }

        void push_back(const T& value) {
            if (full()) {
                throw std::length_error("FixedVector is full");
            }
            items_[size_] = value;
            ++size_;
        }

        T pop_back() {
            if (empty()) {
                throw std::out_of_range("pop_back on an empty FixedVector");
            }
            --size_;
            return items_[size_];
        }

        // Removes the item at position i, keeping the order of the rest.
        void erase_at(std::size_t i) {
            if (i >= size_) {
                throw std::out_of_range("FixedVector::erase_at");
            }
            for (std::size_t j = i; j + 1 < size_; ++j) {
                items_[j] = items_[j + 1];
            }
            --size_;
        }

        // Removes the first item equal to value. Returns false if there was none.
        bool remove(const T& value) {
            for (std::size_t i = 0; i < size_; ++i) {
                if (items_[i] == value) {
                    erase_at(i);
                    return true;
                }
            }
            return false;
        }

        bool contains(const T& value) const {
            for (std::size_t i = 0; i < size_; ++i) {
                if (items_[i] == value) {
                    return true;
                }
            }
            return false;
        }

        void clear() { size_ = 0; }

        // Only the used items count; leftovers past size() are ignored.
        friend bool operator==(const FixedVector& a, const FixedVector& b) {
            if (a.size_ != b.size_) {
                return false;
            }
            for (std::size_t i = 0; i < a.size_; ++i) {
                if (!(a.items_[i] == b.items_[i])) {
                    return false;
                }
            }
            return true;
        }

    private:
        std::array<T, N> items_{};
        std::uint16_t size_ = 0;
    };

} // namespace pokebot
