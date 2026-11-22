# `data/archetypes/` — the lists agents know about

What an agent knows about the decks it might face: one folder per archetype, one decklist per
variant, in the [decklist format](../../src/core/decklist.md#input-format). Agents never get the
opponent's real list; they guess it from these, using the cards they've seen. The library is loaded
by [`ArchetypeLibrary`](../../src/agents/flat_mc/deck_model.md).

Each archetype's `main.txt` is the deck of the same name in [`data/decks/`](../decks/README.md); a
test checks they stay equal. The variants change a few slots, using only cards whose effects are
written.

| Archetype | Variant | Changes from `main` |
|---|---|---|
| `basic-box` | `main` | — |
| | `unfair-stamp` | Unfair Stamp for Hero's Cape (the ACE SPEC); a third Boss's Orders for Glass Trumpet; a third Night Stretcher for Ciphermaniac's Codebreaking |
| | `hammer` | 3 Crushing Hammer for 2 Cyrano and Judge; Budew for Enamorus |
| `dragapult` | `main` | — |
| | `rare-candy` | 3 Rare Candy and a Switch for the 4 Crushing Hammer |
| | `secret-box` | Secret Box for Unfair Stamp (the ACE SPEC); 2 Air Balloon and Judge for 2 Budew and Rosa's Encouragement |
| `mega-greninja` | `main` | — |
| | `hero-cape` | Hero's Cape for Lana's Aid; a seventh Basic {W} Energy for Neo Upper Energy (so Hero's Cape is the ACE SPEC) |
| | `boss` | 2 more Boss's Orders and 2 Night Stretcher for 2 Surfing Beach and 2 Hilda; a second Budew for Psyduck |
| `ns-zoroark` | `main` | — |
| | `unfair-stamp` | Unfair Stamp for Secret Box (the ACE SPEC); Judge for Janine's Secret Art |
| | `hammer` | 3 Crushing Hammer for 2 Binding Mochi and Black Belt's Training; a second Budew for Purrloin |

Every variant uses only the 80 cards of the four decks, so importing them downloads nothing new.

## Adding a list

1. Save it as `data/archetypes/<archetype>/<variant>.txt`. A new archetype is a new folder.
2. Run `pokebot-import data/decks data/archetypes`. Each list must be legal, and every card in it
   must have its effects written: `ArchetypeLibrary::load` rejects the library otherwise.
3. Add a row to the table above.
