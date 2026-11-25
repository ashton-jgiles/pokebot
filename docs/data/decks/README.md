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

**The decks directly in `data/decks/` are the ones that play.** `pokebot-sim --agents` plays every pairing of
them, and tests play random games with them, check each has an archetype folder, and check every card has its
effects. Decks that aren't playable yet wait in `data/decks/incoming/`, which neither reads:

| File | Deck |
|---|---|
| [`incoming/alakazam.txt`](../../../data/decks/incoming/alakazam.txt) | Alakazam (single-Prize) |
| [`incoming/festival-lead.txt`](../../../data/decks/incoming/festival-lead.txt) | Festival Lead: Thwackey, Dipplin (single-Prize) |
| [`incoming/mega-lucario.txt`](../../../data/decks/incoming/mega-lucario.txt) | Mega Lucario ex |
| [`incoming/mega-venasaur.txt`](../../../data/decks/incoming/mega-venasaur.txt) | Mega Venusaur ex, Mega Meganium ex |

## Adding a deck

1. Export the deck from TCG Live (or type it in the same format) and save it as `data/decks/incoming/<name>.txt`.
2. Run `pokebot-import data/decks/incoming/<name>.txt`. Set codes not in
   [`sets.json`](../cards/README.md#setsjson) are found through TCGdex, and older printings use the effects of their
   reprints. The report under **Decks** says how many of its cards are ready and what each other card needs.
3. Check the **Needs review** notes and add effects files where needed, until every card is ready.
4. Move it to `data/decks/<name>.txt` and copy it to `data/archetypes/<name>/main.txt`, with at least two more
   lists in that folder, so agents know the deck (see [archetypes](../archetypes/README.md)). Changing a deck means
   changing that copy too: a test checks they're equal. Adding a playable deck changes what the sim plays, so
   earlier runs no longer repeat.
