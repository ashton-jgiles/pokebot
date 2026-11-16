# `tools/import/http_client` — HTTP requests

**Files:** [`tools/import/http_client.hpp`](../../../tools/import/http_client.hpp), [`tools/import/http_client.cpp`](../../../tools/import/http_client.cpp)
**Library:** `pokebot_import_lib` · **Namespace:** `pokebot::import`

The importer's only network code: "GET this URL and give me the body."

## Types

### `class HttpClient` (interface)

```cpp
virtual std::string get(const std::string& url) = 0;  // throws HttpError on failure
```

Everything else talks to this interface, never to curl directly. That gives two benefits:
- **Tests** use a fake `HttpClient` that returns canned responses, so they never touch the network
  (see `FakeHttp` in [`tests/test_tcgdex.cpp`](../../../tests/test_tcgdex.cpp)).
- **The implementation can be swapped** (e.g. for libcurl or WinHTTP) without changing any other file.

### `class CurlProcessClient`

The real implementation. It runs the system `curl` program, which ships with Windows 10+, macOS
and Linux, and reads its output through a pipe (`_popen` on Windows, `popen` elsewhere).

```
curl --silent --show-error --fail --location --max-time <timeout> "<url>"
```

| Flag | Why |
|---|---|
| `--silent` / `--show-error` | No progress bar, but real errors still show |
| `--fail` | HTTP 4xx/5xx responses make curl exit with an error instead of returning an error page |
| `--location` | Follow redirects |
| `--max-time` | Give up after `timeout_seconds` (default 20) |

`get` throws `HttpError` if curl can't start or exits with a non-zero status.

### `class HttpError`

Derives from `std::runtime_error`.

## Safety

The URL is placed inside a shell command, so `get` first checks it with `is_safe_url`. It must
start with `https://` and contain only letters, digits and `-._~:/?=&%`. Anything else (quotes,
spaces, `;`) is refused with `HttpError`, so a URL can never inject extra shell commands. URLs are
built by our own code from `sets.json` and card ids, so the check should never fire in practice.

## Why not a C++ HTTP library?

Libraries like libcurl or cpp-httplib need TLS libraries that are painful to build with MinGW.
The importer only runs when a deck is added, so starting a `curl` process per request is fast
enough and needs no dependencies.

## Tests

Not tested directly, since that would need the network. Everything that uses it is tested
through the fake client.
