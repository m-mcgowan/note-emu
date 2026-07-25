# sync-wokwi

Playwright-driven sync from local `wokwi/esp32-*/` sources into their hosted Wokwi projects. Manual invocation; skips files that already match; supports `--dry-run`.

## First-time setup

```sh
cd tools/sync-wokwi
npm install
npx playwright install chromium
node sync.js --login
```

`--login` opens a browser window, navigates to wokwi.com, waits for you to log in manually, then saves the auth cookies to `.wokwi-auth.json` (gitignored). Subsequent runs reuse that state, no login prompt.

## Regular use

```sh
# Dry-run: show what would change without saving to Wokwi
node sync.js --dry-run

# Sync all projects
node sync.js

# Sync one project by name (matches substring)
node sync.js bridge

# Debug: keep browser visible + pause on each file
node sync.js --debug
```

## What gets synced

For each project (see `PROJECTS` in `sync.js`):

| Wokwi file | Local source |
|---|---|
| `sketch.ino` | `wokwi/esp32-*/src/main.cpp` |
| `diagram.json` | `wokwi/esp32-*/diagram.json` |
| `secrets.h` | `wokwi/esp32-*/src/secrets.h.example` (placeholders only) |
| `libraries.txt` | `wokwi/esp32-*/libraries.txt` (Wokwi-specific, tracked separately) |

**Safety:** the script refuses to push a `secrets.h` that looks like it contains a real Notehub PAT. The pattern `api_key_...` (base64-ish, >20 chars) triggers a hard skip.

**Verify after save:** after each successful save, the script re-reads the Wokwi file and compares byte-for-byte with the local content. Mismatch aborts the run.

## What isn't synced

- `platformio.ini` — Wokwi doesn't use PIO
- `wokwi.toml` — same
- `sample-output.txt` — output artifact, not source

## Known limitations

- **Owner-only:** the script assumes you own the target Wokwi projects (direct save works). If Wokwi shows a "view-only" state, save fails and the run aborts.
- **UI selectors:** Wokwi's file tabs and save button rely on stable DOM selectors. If Wokwi changes their UI, the script needs updating (see the `wokwi*()` helpers in `sync.js`).
