# `src/agents/flat_mc/deck_model` — what the opponent might be playing

**Files:** [`src/agents/flat_mc/deck_model.hpp`](../../../src/agents/flat_mc/deck_model.hpp), [`src/agents/flat_mc/deck_model.cpp`](../../../src/agents/flat_mc/deck_model.cpp)
**Library:** `pokebot_lib` · **Namespace:** `pokebot`

Agents never get the opponent's decklist (decided 2026-11-18). To deal the opponent's unseen cards
in a [`World`](world.md), they guess the list from a library of known lists, the way a player
recognises Dragapult from a Dreepy and then wonders which version it is. This file holds that
library and the belief over its lists, worked out from the cards seen. Dealing from them is
step 7.3.

```cpp
const ArchetypeLibrary library = ArchetypeLibrary::load("data/archetypes", db);
const DeckModel model(library);
const std::vector<double> belief = model.belief(observation); // one weight per library.lists(), summing to 1
for (const auto& [archetype, weight] : model.by_archetype(belief)) {
    // how likely the opponent is playing each archetype
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

## The belief

A `DeckModel` weighs the library's lists by the opponent's cards the viewer has seen. Over each list
L, with S the cards seen (card c seen k_c times, out of n_c copies in L):

P(L | S) ∝ P(L) · ∏_c f(n_c, k_c), where f(n, k) = C(n, k) if k ≤ n, and ε^(k−n) otherwise.

- **The likelihood is hypergeometric:** the chance of seeing those cards among the list's 60 is
  ∏ C(n_c, k_c) / C(60, K). Every list has 60 cards, so the denominator is the same for all and
  cancels. Cards in a list in more copies are likelier to have been seen, so seeing one copy of a card
  favours a list with 4 over a list with 2 by 4 to 2.
- **ε is the chance of a card no list explains,** per extra copy beyond what the list holds. It
  keeps the belief defined when the opponent plays a list the library doesn't have, or a tech card
  no list has. It's 0.01 by default, so each such copy costs a list a factor of 100.
- **The prior is uniform over archetypes, then over each archetype's variants,** so an archetype
  with more variants isn't made likelier by having them.
- It's worked out in logs (`lgamma`) and normalised, so long games with many cards seen don't
  underflow.

| Member | Behavior |
|---|---|
| `DeckModel(library, unexplained = 0.01)` | Throws `std::invalid_argument` if the library is empty or ε isn't in (0, 1]. It keeps a reference: the library must outlive it |
| `belief(observation)` | The weights for the viewer's opponent, from `opponent_seen` |
| `belief(seen)` | The same from a `SeenCards` (card id → copies seen), such as in tests |
| `by_archetype(belief)` | The weights summed for each archetype, in the library's order |
| `prior(i)` | List i's prior |

`opponent_cards_seen(observation)` counts the opponent cards the viewer has seen by card id. It reads
only `opponent_seen` and those cards' definitions: every card the viewer has seen this game,
wherever it is now, so a Dreepy seen once still counts after it's shuffled back. Nothing else in the
observation, and nothing about the real decklist, goes into the belief.

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
  lists the same;
- with nothing seen, the belief is the prior: archetypes alike, then their variants;
- a card only one archetype plays settles the archetype, and a tech card picks the variant (by
  exactly the ratio the formula gives);
- the number of copies seen counting by the hypergeometric count;
- cards no list explains leaving the belief at the prior, and the belief summing to 1 however many;
- the belief summed by archetype, and bad arguments rejected;
- on the real decks (skipped without generated card data), with the opponent playing each of the
  12 lists in two biased random games: the same cards seen always give exactly the same belief,
  whatever the opponent's real list; after any game of 10 turns or more, the opponent's archetype
  has over 90%, and it's still the likeliest with its exact list taken out of the library.
