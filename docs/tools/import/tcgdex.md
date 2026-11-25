# `tools/import/tcgdex` — TCGdex client

**Files:** [`tools/import/tcgdex.hpp`](../../../tools/import/tcgdex.hpp), [`tools/import/tcgdex.cpp`](../../../tools/import/tcgdex.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

Finds a decklist card in the [TCGdex](https://tcgdex.dev) card database and downloads it, with a
local cache and automatic retries. The API is free, needs no key, and returns JSON from
`https://api.tcgdex.net/v2/en`.

## How a card is found

```
{"POR", 62}
   │  1. set code → TCGdex set id        (data/cards/sets.json:  "POR" → "me03")
   ▼
"me03"
   │  2. GET /sets/me03, find the card whose localId equals 62 as a number
   ▼
"me03-062"
   │  3. GET /cards/me03-062
   ▼
full card JSON
```

Step 2 compares card numbers **as numbers**. TCGdex pads ids (`062`), so the id can't be
built by string formatting. Non-numeric local ids such as `TG01` are skipped.

## Types

### `CardRef`

A card as a decklist names it: `{set_code, number}`, e.g. `{"TWM", 130}`.

### `TcgdexOptions`

| Field | Default | Meaning |
|---|---|---|
| `cache_dir` | `data/cards/raw` | Where downloaded JSON is saved |
| `sets_file` | `data/cards/sets.json` | Set code → TCGdex set id table |
| `refresh` | `false` | Ignore the cache and download again |
| `offline` | `false` | Never use the network; fail if something isn't cached |
| `max_attempts` | `3` | Tries per request |
| `retry_delay` | 500 ms | Wait after a failed try; doubles each time |
| `request_gap` | 150 ms | Pause between requests, to be polite to a free service |
| `page_size` | 250 | Cards per request for a whole set |
| `series` | `sv`, `me` | The series searched for a set code that isn't in `sets.json` |

### `class TcgdexClient`

| Member | Behavior |
|---|---|
| `TcgdexClient(HttpClient& http, TcgdexOptions options)` | Loads `sets.json`. The `HttpClient` must outlive the client |
| `std::string set_id(const std::string& set_code)` | `"TWM"` → `"sv06"`, from `sets.json` or [TCGdex's series](#set-codes). Throws if neither has it |
| `std::string card_id(const CardRef&)` | `{"POR", 62}` → `"me03-062"`. Throws if the number isn't in the set |
| `nlohmann::json card(const CardRef&)` | The full TCGdex card object |
| `nlohmann::json set_cards(const std::string& set_code)` | Every card of a set, from [one request a page](#whole-sets) |
| `std::vector<std::string> set_codes() const` | Every set code in `sets.json`, sorted |
| `int network_requests() const` | Requests made so far, including retries |

Errors are thrown as `std::runtime_error` with a message saying what failed.

## Whole sets

`set_cards` asks TCGdex's GraphQL endpoint (`POST https://api.tcgdex.net/v2/graphql`) for the cards whose id starts with
the set's id and a dash, `page_size` at a time, with only the fields the importer reads. It keeps only cards of that
set, so `me03-` doesn't pick up `me03.5-`, and stops at the first page that isn't full. A response counts once it's
a JSON object with a `data` object and no `errors`, and is retried like any other. A set is a page or two, so all of
Standard is about 25 requests instead of one for each of 3,594 printings.

The fields are TCGdex's own names, but missing values come as `null`, which
[`convert_card`](convert.md) ignores.

## Cache

```
data/cards/raw/
├── sets/me03.json        one file per set
├── bulk/me03.json        a whole set's cards, from set_cards
├── series/sv.json        a series' sets, for set codes
├── set-codes.json        the set codes found
└── cards/me03-062.json   one file per card
```

- A cached file is used instead of the network unless `refresh` is set.
- Within one run, each set is loaded at most once, even with `refresh`.
- Files are written to `<name>.tmp` and then renamed. An interrupted run can't leave a
  half-written file in the cache.
- The cache is in `.gitignore`, because card text belongs to The Pokémon Company.

## Retries

TCGdex occasionally answers `200 OK` with an **empty body**, and requests can fail outright. A
response only counts once it parses as a JSON object. Otherwise the request is retried after
500 ms, then 1 s, up to `max_attempts` tries, and then a `runtime_error` names the URL and the
last problem.

## Set codes

`sets.json` holds the codes of the sets the [coverage report](standard.md) reads. Any other code, such as `PAF` (Paldean
Fates) in an older printing, is found the first time it's needed: `GET /series/sv` and `/series/me` list their sets,
each set's page gives its `abbreviation.official`, and every code found is saved to `raw/set-codes.json`. That's about
30 requests once, and none after: set pages are cached like any other. With `offline`, only the saved codes are used, and the error for an unknown code says to run once online.
If two sets share a code, the first in the series' order wins; `sets.json` decides for its own codes.

## Adding a new set

If a decklist uses a set code that neither `sets.json` nor the series have (a promo code written differently in an
export, say), the import fails with `unknown set code 'XYZ'`. To find the TCGdex id:

1. Open `https://api.tcgdex.net/v2/en/sets` and find the set by name.
2. Open `https://api.tcgdex.net/v2/en/sets/<id>` and check that `abbreviation.official`
   matches the decklist's code.
3. Add `"XYZ": "<id>"` to `sets.json`.

## Tests

[`tests/test_tcgdex.cpp`](../../../tests/test_tcgdex.cpp), tag `[tcgdex]`. It uses a fake
`HttpClient` and a temporary folder, and covers padded ids, unknown sets and numbers, caching,
offline mode, retries after empty or failed responses, and giving up; and whole sets in pages, keeping only the set's
cards, from the cache, offline, and after too many attempts; and set codes found through the series and remembered.
