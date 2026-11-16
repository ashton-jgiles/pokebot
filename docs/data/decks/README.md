# `data/decks/` — decklists

Decklists in the Pokémon TCG Live text export format, one file per deck. The format is described
in [the decklist parser docs](../../src/core/decklist.md#input-format).

| File | Deck | Distinct cards |
|---|---|---|
| [`dragapult.txt`](../../../data/decks/dragapult.txt) | Dragapult ex | 24 |
| [`ns-zoroark.txt`](../../../data/decks/ns-zoroark.txt) | N's Zoroark ex | 29 |

The two decks share 12 cards, for **41 distinct cards** in total.

## Adding a deck

1. Export the deck from TCG Live (or type it in the same format) and save it as `data/decks/<name>.txt`.
2. Run `pokebot-import` on it. If it reports an unknown set code, add the set to
   [`sets.json`](../cards/README.md#setsjson).
3. Check the **Needs review** notes and add effects files where needed.
