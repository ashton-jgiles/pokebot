# `src/agents/deck_model` — what the opponent might be playing

**Files:** [`src/agents/deck_model.hpp`](../../../src/agents/deck_model.hpp), [`src/agents/deck_model.cpp`](../../../src/agents/deck_model.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Agents never get the opponent's decklist (decided 2026-11-18). To deal the opponent's unseen cards
in a [`World`](world.md), they guess the list from a library of known lists, the way a player
recognises Dragapult from a Dreepy and then wonders which version it is. This file holds that
library. Milestone 7 adds the belief over its lists (step 7.2) and dealing from them (step 7.3).

```cpp
const ArchetypeLibrary library = ArchetypeLibrary::load("data/archetypes", db);
for (const ArchetypeList& list : library.lists()) {
    list.count("TWM-128"); // copies of that card in the list
}
```

## The library

The lists are in [`data/archetypes/`](../../data/archetypes/README.md): one folder per archetype,
one decklist file per variant, in the usual [decklist format](../core/decklist.md#input-format).

| Member | Behavior |
|---|---|
| `static ArchetypeLibrary load(dir, db)` | Loads every `<archetype>/<variant>.txt` under `dir`. Other files are ignored, and so are files directly in `dir` (such as a README) |
| `void add(archetype, variant, list, db)` | Adds one list. `load` uses it, and tests add made-up lists with it |
| `lists()` | Every list, sorted by archetype and then by variant |
| `archetypes()` | The archetype names, sorted, each once |
| `find(archetype, variant)` | That list, or `nullptr` |
| `size()`, `empty()` | How many lists |

Each `ArchetypeList` has its `archetype` and `variant` names, the `Decklist` as written, and `cards`:
one `ArchetypeCard{def, count}` per distinct card, sorted by card id, with entries for the same card
added together. `count(id)` gives a card's copies, or 0.

The lists point at the `CardDB`'s definitions, so the `CardDB` must outlive the library.

## What a list must be

`add`, and so `load`, throws `ArchetypeError` with the list's name (`archetype/variant`) when the list:

- isn't a legal deck, by the [deck validator](../cards/validator/deck_validator.md): every problem
  is in the message;
- has a card whose effects aren't written ([`missing_effects`](../cards/db/card_db.md#missing_effects)):
  a world dealt from it would hold a card the engine can't play properly;
- has the same archetype and variant as a list already added.

`load` also throws when `dir` isn't a folder, when it holds no archetypes, when an archetype folder
has no lists, and when a file doesn't parse (the message names the file and the line).

## Why it's shaped this way

- **The library is separate from the decks the games use.** `data/decks/` is what the match harness
  deals to each player; `data/archetypes/` is what an agent knows about decks in general. Nothing
  passes a game's decklist to an agent, so the only way an agent learns what the opponent plays is
  the cards it sees. The real decks are in the library too, as each archetype's `main` list, the
  way a well-known list is public knowledge. Removing a list from the library (or playing one that
  isn't in it) tests how the agents cope with a list they don't know.
- **Variants give the belief something to tell apart.** The four decks share many cards (Meowth ex,
  Fezandipiti ex, Budew, Ultra Ball, Boss's Orders, Buddy-Buddy Poffin, Lillie's Determination,
  Night Stretcher, Poké Pad, Special Red Card), so early in a game the archetype isn't settled.
  Each archetype's variants change a few slots, often the ACE SPEC, so a tech card seen later can
  separate them.
- **Folders, not a new file format.** The decklist parser and validator are reused, and
  `pokebot-import data/decks data/archetypes` imports and checks every list.

## Tests

[`tests/test_deck_model.cpp`](../../../tests/test_deck_model.cpp), tag `[deck_model]`:

- a list's cards counted by card id, with entries for the same card added together;
- lists kept in order by archetype and then variant, `archetypes()` and `find`;
- illegal lists, unknown cards, cards without effects and a list added twice rejected, with the
  list's name and the problem in the message;
- loading from a folder per archetype, ignoring other files, and each way loading fails;
- the project's library (skipped without generated card data): an archetype for every deck in
  `data/decks/`, each with a `main` list equal to that deck and at least two variants, and no two
  lists the same.
