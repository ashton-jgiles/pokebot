#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include "engine/shared/fixed_vector.hpp"

namespace pokebot {

    class Rng {
    public:
        explicit Rng(std::uint64_t seed = 0) {
            for (auto& word : state_) {
                seed += 0x9E3779B97F4A7C15ULL;
                std::uint64_t z = seed;
                z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
                z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
                word = z ^ (z >> 31);
            }
        }

        std::uint64_t next() {
            const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
            const std::uint64_t t = state_[1] << 17;
            state_[2] ^= state_[0];
            state_[3] ^= state_[1];
            state_[1] ^= state_[2];
            state_[0] ^= state_[3];
            state_[2] ^= t;
            state_[3] = rotl(state_[3], 45);
            return result;
        }

        std::uint32_t below(std::uint32_t n) {
            if (n == 0) {
                throw std::invalid_argument("Rng::below(0)");
            }
            const std::uint64_t bound = n;
            const std::uint64_t threshold = (std::uint64_t{0} - bound) % bound;
            while (true) {
                const std::uint64_t r = next();
                if (r >= threshold) {
                    return static_cast<std::uint32_t>(r % bound);
                }
            }
        }

        // true = heads
        bool coin_flip() { return (next() >> 63) != 0; }

        template <class T, std::size_t N>
        void shuffle(FixedVector<T, N>& items) {
            for (std::size_t i = items.size(); i > 1; --i) {
                const std::size_t j = below(static_cast<std::uint32_t>(i));
                std::swap(items[i - 1], items[j]);
            }
        }

        bool operator==(const Rng&) const = default;

    private:
        static std::uint64_t rotl(std::uint64_t x, int k) {
            return (x << k) | (x >> (64 - k));
        }

        std::array<std::uint64_t, 4> state_{};
    };

} // namespace pokebot
