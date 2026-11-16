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

### `class TcgdexClient`

| Member | Behavior |
|---|---|
| `TcgdexClient(HttpClient& http, TcgdexOptions options)` | Loads `sets.json`. The `HttpClient` must outlive the client |
| `std::string set_id(const std::string& set_code) const` | `"TWM"` → `"sv06"`. Throws if the code isn't in `sets.json` |
| `std::string card_id(const CardRef&)` | `{"POR", 62}` → `"me03-062"`. Throws if the number isn't in the set |
| `nlohmann::json card(const CardRef&)` | The full TCGdex card object |
| `int network_requests() const` | Requests made so far, including retries |

Errors are thrown as `std::runtime_error` with a message saying what failed.

## Cache

```
data/cards/raw/
├── sets/me03.json        one file per set
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

## Adding a new set

If a decklist uses a set code that isn't in `sets.json`, the import fails with
`unknown set code 'XYZ' - add it to data/cards/sets.json`. To find the TCGdex id:

1. Open `https://api.tcgdex.net/v2/en/sets` and find the set by name.
2. Open `https://api.tcgdex.net/v2/en/sets/<id>` and check that `abbreviation.official`
   matches the decklist's code.
3. Add `"XYZ": "<id>"` to `sets.json`.

## Tests

[`tests/test_tcgdex.cpp`](../../../tests/test_tcgdex.cpp), tag `[tcgdex]`. It uses a fake
`HttpClient` and a temporary folder, and covers padded ids, unknown sets and numbers, caching,
offline mode, retries after empty or failed responses, and giving up.
