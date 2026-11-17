# `src/engine/shared/rng.hpp` — random number generator

**File:** [`src/engine/shared/rng.hpp`](../../../../src/engine/shared/rng.hpp) (header-only) · **Namespace:** `pokebot`

Every random event in a game goes through `Rng`: shuffling, coin flips, and choosing which
face-down Prize card is taken. It lives inside `GameState`, so **the same seed and the same choices
always produce the same game**.

The algorithm is **xoshiro256\*\*** (Blackman & Vigna), seeded through splitmix64. It's fast and
has only 32 bytes of state. `std::mt19937` keeps about 5 KB, which would make every game-state copy
several times bigger.

## Interface

| Member | Behavior |
|---|---|
| `explicit Rng(std::uint64_t seed = 0)` | Same seed, same sequence |
| `std::uint64_t next()` | The next 64 random bits |
| `std::uint32_t below(std::uint32_t n)` | Uniform in `[0, n)`, using rejection sampling to avoid the bias of `% n`. Throws for `n == 0` |
| `bool coin_flip()` | `true` means heads |
| `shuffle(FixedVector&)` | Fisher–Yates shuffle: every order equally likely |
| `==` | Two generators with equal state produce equal sequences |

The agents use their own `Rng`, seeded separately, so an agent's choices never disturb the
game's own random events.

## Tests

[`tests/test_engine_state.cpp`](../../../../tests/test_engine_state.cpp), tag `[engine]`: same seed
repeats, `below` covers its range evenly, and shuffles are repeatable permutations.
