# Native Demo

Desktop demo that exercises the softcard protocol using libcurl. No embedded hardware required.

## Prerequisites

- C compiler (gcc or clang)
- libcurl development headers (`brew install curl` on macOS, `apt install libcurl4-openssl-dev` on Debian/Ubuntu)
- A Notehub Personal Access Token (PAT) — generate one at https://notehub.io under **Account Settings > API Tokens**

## Build

```sh
make
```

## Usage

Set the `NOTEHUB_PAT` environment variable and run:

```sh
export NOTEHUB_PAT="your-notehub-pat"
./note-emu-demo
```

### Basic mode (default)

Sends `card.version`, `card.temp`, and `card.status` requests and prints the responses:

```sh
./note-emu-demo
```

### Project mode

Assigns the softcard to a Notehub project, sends a test note via `note.add`, and confirms the project assignment with `hub.get`:

```sh
export NOTEHUB_PRODUCT_UID="com.your-company:your-product"
./note-emu-demo project
```

### Environment variables

| Variable | Required | Description |
|---|---|---|
| `NOTEHUB_PAT` | Yes | Notehub Personal Access Token |
| `NOTEHUB_PRODUCT_UID` | Project mode only | Product UID for `hub.set` |
| `SOFTCARD_URL` | No | Override the softcard service URL |

## Expected output

```
note-emu demo

Resolving account UID from token...
Connected to https://softcard.blues.com

Test: basic

  >> {"req":"card.version"}
  << {"version":"...","device":"dev:soft:...","body":{...}}
  >> {"req":"card.temp"}
  << {"value":23.00,...}
  >> {"req":"card.status"}
  << {"status":"...","storage":...}

PASS (3/3 requests succeeded)
```

## Integration tests

The `tests/` directory at the project root contains a Python verifier (`test_softcard.py`) that builds and runs this demo, then confirms events arrive in Notehub via the REST API. Run it with:

```sh
./tests/run.sh          # all tests
./tests/run.sh basic    # basic test only
./tests/run.sh project  # project + event verification
```
