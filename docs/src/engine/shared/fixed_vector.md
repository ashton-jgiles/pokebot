# `src/engine/shared/fixed_vector.hpp` — fixed-capacity list

**File:** [`src/engine/shared/fixed_vector.hpp`](../../../../src/engine/shared/fixed_vector.hpp) (header-only) · **Namespace:** `pokebot`

```cpp
template <class T, std::size_t N>
class FixedVector;
```

A list of at most `N` items stored **inside the object**, with no heap allocation. Copying it is a
plain memory copy. Every zone in the game state (deck, hand, Bench, attached Energy…) is one, which
is why a whole `GameState` copies in nanoseconds.

## Interface

Works like a small `std::vector`:

| Member | Behavior |
|---|---|
| `FixedVector{a, b, c}` | Builds one from a list; throws `std::length_error` if it has more than `N` items |
| `size()`, `empty()`, `full()`, `capacity()` | Sizes; `capacity()` is `N` |
| `operator[]`, `front()`, `back()` | Element access (unchecked, like `std::vector`) |
| `begin()`, `end()` | Range-for support |
| `push_back(value)` | Appends; throws `std::length_error` when full |
| `pop_back()` | Removes and **returns** the last item; throws `std::out_of_range` when empty |
| `erase_at(i)` | Removes item `i`, keeping the order of the rest |
| `remove(value)` | Removes the first equal item; returns whether one was found |
| `contains(value)` | Whether an equal item exists |
| `clear()` | Empties it |
| `==` | Compares only the items in use, not leftover slots past `size()` |

Going past the capacity **throws instead of corrupting memory**. If the engine ever tries to put a
sixth Pokémon on the Bench, that's a rules bug, and the stress tests surface it as an exception.

`N` must fit in 16 bits; the size is stored as a `std::uint16_t`.

## Tests

[`tests/test_engine_state.cpp`](../../../../tests/test_engine_state.cpp), tag `[engine]`.
