# `data/decks/` — decklists

Decklists in the Pokémon TCG Live text export format, one file per deck. The format is described
in [the decklist parser docs](../../src/core/decklist.md#input-format).

| File | Deck | Distinct cards |
|---|---|---|
| [`dragapult.txt`](../../../data/decks/dragapult.txt) | Dragapult ex | 24 |
| [`ns-zoroark.txt`](../../../data/decks/ns-zoroark.txt) | N's Zoroark ex | 29 |
| [`basic-box.txt`](../../../data/decks/basic-box.txt) | Basic Box (Mega Kangaskhan ex, Ogerpon ex, Raging Bolt ex …) | 29 |
| [`mega-greninja.txt`](../../../data/decks/mega-greninja.txt) | Mega Greninja ex | 30 |

All four decks together use **80 distinct cards**.

## Adding a deck

1. Export the deck from TCG Live (or type it in the same format) and save it as `data/decks/<name>.txt`.
2. Run `pokebot-import` on it. If it reports an unknown set code, add the set to
   [`sets.json`](../cards/README.md#setsjson).
3. Check the **Needs review** notes and add effects files where needed.
