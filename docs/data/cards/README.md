# `data/cards/` — card data

| Path | Contents | Written by | In git? |
|---|---|---|---|
| `sets.json` | Set code → TCGdex set id | hand | yes |
| `corrections.json` | Fixes to TCGdex's data | hand | yes |
| `raw/` | Unmodified TCGdex responses (cache) | `pokebot-import` | **no** |
| `generated/` | Cards in our format | `pokebot-import` | **no** |
| `effects/` | Hand-written effects, rules and labels | hand | yes |

`raw/` and `generated/` contain card text, which belongs to The Pokémon Company, so they stay
out of git. Anyone who clones the repo recreates them by running `pokebot-import`.

## `sets.json`

Maps the set codes used in decklists to TCGdex set ids:

```json
{
  "TWM": "sv06",
  "POR": "me03",
  "MEE": "mee"
}
```

Each entry was checked against TCGdex's `abbreviation.official` field. To add a set, see
[Adding a new set](../../tools/import/tcgdex.md#adding-a-new-set).

## `corrections.json`

Fixes to what TCGdex gets wrong, applied to its card before it's converted, in TCGdex's own field names:

```json
{
  "cards": { "30th-021": { "stage": "Stage2", "evolveFrom": "Frogadier" } },
  "evolves_from": { "Arcanine": "Growlithe" }
}
```

- `cards`: by TCGdex id; the fields given replace the card's. The ones there are the six Pokémon ex of 30th
  Celebration that TCGdex stores as Basic, checked on Limitless.
- `evolves_from`: by card name, used only for a Stage 1 or Stage 2 whose `evolveFrom` is empty.

The importer stops if the file is missing. A test checks that Greninja ex (30C 21) comes out as a Stage 2.

## `generated/<id>.json` — our card format

One file per card, named by our id: decklist set code + number, e.g. `TWM-130.json`. Written
by `to_json` in [`card.cpp`](../../src/cards/card/card.md) and read back by `from_json`.

### Pokémon

```json
{
  "id": "TWM-130",
  "tcgdex_id": "sv06-130",
  "name": "Dragapult ex",
  "kind": "pokemon",
  "rules": ["ex"],
  "stage": "stage2",
  "evolves_from": "Drakloak",
  "hp": 320,
  "types": ["dragon"],
  "weakness": null,
  "resistance": null,
  "retreat": 1,
  "abilities": [],
  "attacks": [
    { "name": "Jet Headbutt", "cost": ["colorless"], "damage": 70,
      "damage_mod": "none", "text": "", "effects": [] },
    { "name": "Phantom Dive", "cost": ["fire", "psychic"], "damage": 200,
      "damage_mod": "none", "text": "Put 6 damage counters on …", "effects": [] }
  ]
}
```

`weakness` / `resistance` are `null` or an object:
`{ "type": "fighting", "multiplier": 2 }` / `{ "type": "fighting", "amount": 30 }`.

### Trainer

```json
{
  "id": "MEG-131", "tcgdex_id": "me01-131", "name": "Ultra Ball",
  "kind": "trainer", "rules": [], "trainer_kind": "item",
  "text": "You can use this card only if you discard 2 other cards …", "effects": []
}
```

### Energy

```json
{
  "id": "MEE-7", "tcgdex_id": "mee-007", "name": "Darkness Energy",
  "kind": "energy", "rules": [], "energy_kind": "basic", "provides": ["darkness"],
  "text": "", "effects": []
}
```

### Allowed values

| Field | Values |
|---|---|
| `kind` | `pokemon`, `trainer`, `energy` |
| `rules` | any of `ex`, `mega`, `tera`, `ace_spec` |
| `stage` | `basic`, `stage1`, `stage2` |
| energy types | `grass`, `fire`, `water`, `lightning`, `psychic`, `fighting`, `darkness`, `metal`, `fairy`, `dragon`, `colorless` |
| `damage_mod` | `none`, `plus`, `minus`, `times` |
| `trainer_kind` | `item`, `supporter`, `stadium`, `tool` |
| `energy_kind` | `basic`, `special` |

Any other value is rejected when the card is loaded.

## `effects/<id>.json` — hand-written effects

Optional, one per card, with the same file name as the generated card. It's merged on top of the
generated card when the card database loads (see [`merge_effects`](../../src/cards/db/card_db.md)):

```json
{
  "rules": ["ex", "tera"],
  "attacks": {
    "Shinobi Blade": {
      "effects": [{ "op": "may", "effects": [{ "op": "search_deck", "count": 1 }] }]
    }
  }
}
```

- `attacks` / `abilities`: objects keyed by **name**. Their keys are merged into the matching
  attack or ability. A name that doesn't exist is an error.
- Any other key replaces the generated value. Use this for corrections, such as adding `tera`.
  `rules` is replaced **as a whole**, so list every rule: `["ex", "tera"]`, not just `["tera"]`.

### Recording Tera

TCGdex doesn't say which Pokémon ex are Tera, so it's recorded here after checking the printed
card (Tera cards show a **Tera** label and a bench-protection rule under the name):

| Card is… | Effects file |
|---|---|
| Tera | `{ "rules": ["ex", "tera"] }` |
| Not Tera | `{ "rules": ["ex"] }`, which marks it as checked |
| Mega ex | nothing; never Tera, and never flagged |

`pokebot-import` reminds you about any non-Mega Pokémon ex whose effects file doesn't set `rules` yet.
As of 2026-11-16 all 14 non-Mega Pokémon ex in the four decks are recorded. The Tera ones are
TWM-130 Dragapult ex, TWM-106 Greninja ex, TWM-25 Teal Mask Ogerpon ex, PRE-27 Wellspring Mask
Ogerpon ex and ASC-121 Koraidon ex.
- An effects file with no matching generated card is an error, which catches typos in file names.

Attack `effects` are lists of building blocks (`draw`, `search_deck`, `may`, …), listed with
their fields in [the effect language](../../src/cards/effects/program.md). They're compiled when
the cards load, so a typo is an error naming the card and attack. Ability, Trainer and Energy
effects come in later Milestone 3 steps; the importer reports how many still have none.

Attacks with effects so far: Froakie's Collect, Frogadier's Summoning Jutsu, Purrloin's Invite
Evil, Greninja ex's Shinobi Blade, Raging Bolt ex's Burst Roar, Chien-Pao's Icicle Loop and
Dunsparce's Trading Places, Mega Froslass ex's Absolute Snow (Asleep) and Munkidori's Mind Bend
(Confused), and the damage formulas of Mega Froslass ex (Resentful Refrain), N's Reshiram
(Powerful Rage), N's Darmanitan (Back Draft), Lillie's Clefairy ex (Full Moon Rondo), Pecharunt ex
(Irritated Outburst), Teal Mask Ogerpon ex (Myriad Leaf Shower), Koraidon ex (Orichalcum Fang),
Enamorus (Love Resonance), Mega Greninja ex (Ninja Spinner), Mega Kangaskhan ex (Rapid-Fire Combo)
and Raging Bolt ex (Bellowing Thunder). With targets and copying: Dragapult ex (Phantom Dive),
N's Darmanitan (Flamebody Cannon), Wellspring Mask Ogerpon ex (Torrential Pump), Greninja ex
(Mirage Barrage, Stealthy Slash), Fezandipiti ex (Cruel Arrow), Meowth ex (Tuck Tail) and N's
Zoroark ex (Night Joker). With lasting effects: Koraidon ex (Impact Blow), N's Zekrom (Rampaging
Thunder), Latias ex (Eon Blade), Iron Leaves ex (Prism Edge), Yveltal (Clutch), Wellspring Mask
Ogerpon ex (Sob) and Budew (Itchy Pollen). N's Zekrom's Shred ignores effects on the Defending Pokémon.

A Tool, a Stadium or a passive Ability has a [`"passive"`](../../src/cards/effects/program.md#always-on-effects-passive)
list instead:

```json
{ "passive": [{ "modify": "retreat_cost", "add": -2 }] }
```

With passive effects so far: Air Balloon, Hero's Cape and Binding Mochi (Tools), N's Castle
(Stadium), and Latias ex's Skyliner and Lillie's Clefairy ex's Fairy Zone (Abilities).

An Ability you choose to use has `effects`, optional `requires`, and `active_only`, `one_per_name`
or `once_per_turn` (see [Abilities](../../src/engine/actions/abilities.md)). So far: N's Zoroark ex
(Trade), Mega Kangaskhan ex (Run Errand), Fezandipiti ex (Flip the Script), Pecharunt ex
(Subjugating Chains), Dudunsparce (Run Away Draw), Drakloak (Recon Directive), Teal Mask Ogerpon ex
(Teal Dance), Munkidori (Adrena-Brain) and Mega Greninja ex (Mortal Shuriken), plus Surfing Beach's
switch. Adrena-Brain always moves as many counters as it can, up to 3.

Triggered (`"trigger"`): Meowth ex (Last-Ditch Catch), Chien-Pao (Snow Sink) and Iron Leaves ex
(Rapid Vernier) when played to the Bench, and Risky Ruins.

Special Energy ([`provides` and `provides_when`](../../src/cards/effects/program.md#special-energy)):
Neo Upper Energy (two of any type on a Stage 2) and Ignition Energy ({C}{C}{C} on an Evolution
Pokémon, discarded at the end of your turn).

One-of-a-kind cards name their [C++ code](../../src/engine/effects/custom.md) with `"custom"`:
Rare Candy and Transformation Tome. Area Zero Underdepths is a `bench_size` passive with
`"if_tera_in_play": true`.

An Ability that can't change anything with the cards we have says why with `no_effect`, and
doesn't count as missing. Only Psyduck's Damp does: no card in our pool has an Ability that Knocks
Out its own Pokémon.

```json
{ "abilities": { "Damp": { "no_effect": "No Ability in our card pool requires its Pokemon to Knock Out itself" } } }
```


An Item or Supporter has a top-level `effects` list, and optionally `requires`: the
[conditions](../../src/cards/effects/program.md#conditions) that must hold before it can be played.

```json
{
  "requires": [{ "hand_besides": 2 }],
  "effects": [
    { "op": "discard_from_hand", "count": 2 },
    { "op": "search_deck", "count": 1, "filter": { "kind": "pokemon" } }
  ]
}
```

A search whose text says "reveal" sets `"reveal": true`, so the opponent sees what was found; the
coverage test fails for any that doesn't. So far: Ultra Ball, Poké Pad (ASC and POR), Dawn, Hilda,
Cyrano, Secret Box, Crispin, Meowth ex's Last-Ditch Catch, Frogadier's Summoning Jutsu and Purrloin's
Invite Evil.

Trainers with effects so far: Switch, Boss's Orders, Ultra Ball, Night Stretcher, Energy
Retrieval, Poké Pad (ASC and POR), Cyrano, Lillie's Determination, Judge, Unfair Stamp, Hilda, Dawn,
Secret Box, Buddy-Buddy Poffin, Lana's Aid, Special Red Card, Ciphermaniac's Codebreaking and
Crushing Hammer, Black Belt's Training, and the Energy Trainers: N's PP Up, Rosa's Encouragement,
Glass Trumpet, Janine's Secret Art, Energy Switch and Crispin. Crispin's two Energy aren't checked to
be of different types.

## `raw/`

```
raw/sets/<tcgdex set id>.json
raw/cards/<tcgdex card id>.json
```

Exactly what TCGdex returned. See [the TCGdex client's cache](../../tools/import/tcgdex.md#cache).
