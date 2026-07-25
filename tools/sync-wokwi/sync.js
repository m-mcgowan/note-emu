// sync-wokwi — Playwright-driven sync from local wokwi/esp32-* sources
// into their hosted Wokwi projects.
//
// See README.md for usage.

import { chromium } from 'playwright';
import fs from 'node:fs';
import path from 'node:path';
import readline from 'node:readline/promises';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const REPO_ROOT = path.resolve(__dirname, '..', '..');
const AUTH_STATE_FILE = path.join(__dirname, '.wokwi-auth.json');

// Project catalog — Wokwi project ID → local sources.
// `wokwi` is the filename inside the hosted project; `local` is relative to
// the repo root. To add/remove files: edit this list.
const PROJECTS = [
    {
        id: '465203727626487809',
        name: 'esp32-notec',
        files: [
            { wokwi: 'sketch.ino',    local: 'wokwi/esp32-notec/src/main.cpp',              adapt: 'sketch' },
            { wokwi: 'diagram.json',  local: 'wokwi/esp32-notec/diagram.json' },
            { wokwi: 'secrets.h',     local: 'wokwi/esp32-notec/src/secrets.h.example' },
            { wokwi: 'libraries.txt', local: 'wokwi/esp32-notec/libraries.txt', optional: true },
        ],
    },
    {
        id: '469739119860047873',
        name: 'esp32-notecpp',
        files: [
            { wokwi: 'sketch.ino',    local: 'wokwi/esp32-notecpp/src/main.cpp',              adapt: 'sketch' },
            { wokwi: 'diagram.json',  local: 'wokwi/esp32-notecpp/diagram.json' },
            { wokwi: 'secrets.h',     local: 'wokwi/esp32-notecpp/src/secrets.h.example' },
            { wokwi: 'libraries.txt', local: 'wokwi/esp32-notecpp/libraries.txt', optional: true },
        ],
    },
    {
        id: '470467610960276481',
        name: 'esp32-bridge',
        files: [
            { wokwi: 'sketch.ino',    local: 'wokwi/esp32-bridge/src/main.cpp',              adapt: 'sketch' },
            { wokwi: 'diagram.json',  local: 'wokwi/esp32-bridge/diagram.json' },
            { wokwi: 'secrets.h',     local: 'wokwi/esp32-bridge/src/secrets.h.example' },
            { wokwi: 'libraries.txt', local: 'wokwi/esp32-bridge/libraries.txt', optional: true },
        ],
    },
];

// ────────────────────────────────────────────────────────────────────────────
// Safety: refuse to push a secrets.h that looks like it contains a real PAT.
// Notehub PATs look like `api_key_<40+ base64 chars>`. Placeholder is
// literally "your-notehub-api-token".
// ────────────────────────────────────────────────────────────────────────────
function containsRealPat(text) {
    // Only inspect the NOTEHUB_PAT line.
    const m = text.match(/#define\s+NOTEHUB_PAT\s+"([^"]+)"/);
    if (!m) return false;
    const value = m[1];
    if (value === 'your-notehub-api-token') return false;
    // Anything else base64-shaped is probably real.
    return /^api_key_[A-Za-z0-9+/=]{20,}$/.test(value) || value.length > 30;
}

// ────────────────────────────────────────────────────────────────────────────
// Adapt sketch source for Wokwi (Arduino sketch environment).
// PIO's `src/main.cpp` uses `<Arduino.h>` at the top; Wokwi's Arduino build
// already implicitly includes Arduino.h, and .ino files typically don't need
// it. We leave the include in — harmless — but this hook exists so we can
// tweak per-project later without editing sketches manually.
// ────────────────────────────────────────────────────────────────────────────
function adaptForWokwi(content, kind) {
    if (kind !== 'sketch') return content;
    // Currently a no-op transform. Keep the raw contents.
    return content;
}

// ────────────────────────────────────────────────────────────────────────────
// Auth: first-run login flow.
//
// Google's OAuth flow detects Playwright's Chromium via `navigator.webdriver`
// and refuses login ("this browser or app may not be secure"). We work
// around that with:
//   - `channel: 'chrome'` — use the real Chrome install rather than
//     Playwright's Chromium
//   - `--disable-blink-features=AutomationControlled` — masks the automation
//     flag
//   - a persistent user-data-dir — behaves like a normal browser session
// ────────────────────────────────────────────────────────────────────────────
const USER_DATA_DIR = path.join(__dirname, '.chrome-profile');

async function launchAuthedContext({ headless }) {
    // Persistent context is what tricks Google into treating this as a
    // normal browser. Storage state file is used only for the SECOND flow
    // (fresh browsers per run) — with the persistent profile, cookies live
    // in the user-data-dir directly.
    return chromium.launchPersistentContext(USER_DATA_DIR, {
        channel: 'chrome',
        headless,
        args: [
            '--disable-blink-features=AutomationControlled',
            '--disable-features=IsolateOrigins,site-per-process',
        ],
        ignoreDefaultArgs: ['--enable-automation'],
    });
}

async function firstRunLogin() {
    console.log('First-time setup: opening Chrome to wokwi.com — please log in.');
    console.log('After you\'re logged in and can see your project list, come back to this terminal.\n');
    console.log('Using a persistent Chrome profile at:', path.relative(REPO_ROOT, USER_DATA_DIR));

    const context = await launchAuthedContext({ headless: false });
    const page = context.pages()[0] || await context.newPage();
    await page.goto('https://wokwi.com/login');

    const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
    await rl.question('\nPress Enter here after you have logged in and Wokwi loads your project list… ');
    rl.close();

    // Also save storageState for potential future use, but the persistent
    // profile is what will actually be re-used on subsequent runs.
    await context.storageState({ path: AUTH_STATE_FILE });
    await context.close();
    console.log(`Login complete. Profile persisted at ${path.relative(REPO_ROOT, USER_DATA_DIR)}`);
}

// ────────────────────────────────────────────────────────────────────────────
// Wokwi UI helpers — driving the file tabs + Monaco editor.
// These are the parts most likely to break if Wokwi changes their UI.
// ────────────────────────────────────────────────────────────────────────────

// Wokwi project pages use Monaco Editor. Each open file has a tab. Tabs are
// visible in the sidebar; clicking one changes which file the editor shows.
// We drive edits via Monaco's own JS API instead of typing keystrokes — much
// more reliable.

async function openProject(context, projectId) {
    const page = await context.newPage();
    await page.goto(`https://wokwi.com/projects/${projectId}`);
    // Wait for Monaco editor to initialize.
    await page.waitForSelector('.monaco-editor', { timeout: 30_000 });
    // Give the file tabs a moment to render.
    await page.waitForTimeout(2_000);
    return page;
}

// List the filenames currently in the project's file tab bar.
// We fall back to the Monaco model list if the tabs aren't found.
async function listWokwiFiles(page) {
    return page.evaluate(() => {
        // Look for tab-like elements with filenames.
        const tabs = [
            ...document.querySelectorAll('[role="tab"], .file-tab, [data-testid*="file"]'),
        ];
        const names = tabs.map(t => t.textContent?.trim()).filter(Boolean);
        if (names.length) return names;

        // Fallback: ask Monaco for all its models.
        if (window.monaco?.editor) {
            return window.monaco.editor.getModels().map(m => {
                const uri = m.uri.toString();
                return uri.split('/').pop();
            });
        }
        return [];
    });
}

// Click the tab for a given filename. Returns true if we found and clicked it.
async function clickWokwiTab(page, filename) {
    // Try a few common tab selectors.
    const selectors = [
        `[role="tab"]:has-text("${filename}")`,
        `.file-tab:has-text("${filename}")`,
        `[data-testid*="file"]:has-text("${filename}")`,
        `text="${filename}"`,
    ];
    for (const sel of selectors) {
        try {
            const el = page.locator(sel).first();
            if (await el.isVisible({ timeout: 1_000 }).catch(() => false)) {
                await el.click();
                await page.waitForTimeout(300);
                return true;
            }
        } catch {}
    }
    return false;
}

// Get the current content of the active Monaco editor.
async function getEditorContent(page) {
    return page.evaluate(() => {
        if (!window.monaco?.editor) return null;
        const editors = window.monaco.editor.getEditors?.() || [];
        // Pick the focused/active editor, or the first visible one.
        const active = editors.find(e => e.hasTextFocus()) || editors[0];
        return active?.getValue() ?? null;
    });
}

// Set the active Monaco editor's content.
async function setEditorContent(page, content) {
    await page.evaluate((newContent) => {
        if (!window.monaco?.editor) throw new Error('monaco not available');
        const editors = window.monaco.editor.getEditors?.() || [];
        const active = editors.find(e => e.hasTextFocus()) || editors[0];
        if (!active) throw new Error('no active editor');
        active.setValue(newContent);
    }, content);
    // Give Monaco a moment to register the change (for the "modified" dot).
    await page.waitForTimeout(300);
}

// Save. Wokwi's save is Cmd+S (macOS) / Ctrl+S (other). Also has a button.
async function saveProject(page) {
    const isMac = process.platform === 'darwin';
    await page.keyboard.press(isMac ? 'Meta+s' : 'Control+s');
    // Wait for the save to complete — Wokwi typically shows a "saved"
    // indicator or the modified-dot disappears. Give it a couple seconds.
    await page.waitForTimeout(2_000);
}

// ────────────────────────────────────────────────────────────────────────────
// Project-lock handling.
//
// Wokwi projects can be "locked" from a dropdown menu (the "Lock project"
// menu item is a checkbox — checked = locked). Locked projects silently
// drop Cmd+S saves. We need to unlock before editing and re-lock after.
//
// The menu is opened from a dropdown button near the "Save" button — we
// try a handful of selectors and fall through if nothing works.
// ────────────────────────────────────────────────────────────────────────────

async function openLockMenu(page) {
    const triggers = [
        // Common Material UI patterns for a split/menu-adjacent button
        'button[aria-label*="save options" i]',
        'button[aria-label*="more" i]',
        'button[aria-haspopup="true"]',
        'button:has(svg[data-testid="ArrowDropDownIcon"])',
        'button:has(svg[data-testid="MoreVertIcon"])',
        // Save button variants
        'button:has-text("Save")',
    ];
    for (const sel of triggers) {
        const buttons = page.locator(sel);
        const count = await buttons.count().catch(() => 0);
        for (let i = 0; i < count; i++) {
            const btn = buttons.nth(i);
            if (!(await btn.isVisible({ timeout: 300 }).catch(() => false))) continue;
            await btn.click().catch(() => {});
            const item = page.locator('[role="menuitem"]:has-text("Lock project")').first();
            if (await item.isVisible({ timeout: 800 }).catch(() => false)) {
                return item;
            }
            // Close any wrong menu we opened
            await page.keyboard.press('Escape').catch(() => {});
        }
    }
    return null;
}

// Ensure the project is unlocked. Returns true if we changed the state
// (so the caller knows to re-lock afterwards).
async function unlockProjectIfNeeded(page) {
    const item = await openLockMenu(page);
    if (!item) {
        console.log('  [warn] could not find "Lock project" menu — proceeding without unlock');
        return false;
    }
    const checkbox = item.locator('input[name="lock"]');
    const isLocked = await checkbox.isChecked().catch(() => false);
    if (isLocked) {
        console.log('  unlocking project…');
        await item.click();
        await page.waitForTimeout(500);
    }
    await page.keyboard.press('Escape');
    await page.waitForTimeout(300);
    return isLocked;
}

// Re-lock the project.
async function relockProject(page) {
    const item = await openLockMenu(page);
    if (!item) {
        console.log('  [warn] could not find "Lock project" menu to re-lock');
        return;
    }
    const checkbox = item.locator('input[name="lock"]');
    const isLocked = await checkbox.isChecked().catch(() => false);
    if (!isLocked) {
        console.log('  re-locking project…');
        await item.click();
        await page.waitForTimeout(500);
    }
    await page.keyboard.press('Escape');
    await page.waitForTimeout(300);
}

// ────────────────────────────────────────────────────────────────────────────
// Main sync loop.
// ────────────────────────────────────────────────────────────────────────────
async function syncProject(context, project, opts) {
    console.log(`\n━━━ ${project.name} (${project.id}) ━━━`);

    const page = await openProject(context, project.id);
    if (opts.debug) {
        console.log('  [debug] paused — inspect the page, then press Enter…');
        const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
        await rl.question('');
        rl.close();
    }

    const wokwiFiles = await listWokwiFiles(page);
    console.log(`  files on wokwi: ${wokwiFiles.join(', ') || '(none detected — UI may have changed)'}`);

    // Unlock the project before we try any writes.
    const weUnlocked = opts.dryRun ? false : await unlockProjectIfNeeded(page);

    let changed = 0, unchanged = 0, skipped = 0, verifyFailed = 0;

    for (const file of project.files) {
        const localPath = path.join(REPO_ROOT, file.local);
        if (!fs.existsSync(localPath)) {
            if (file.optional) {
                console.log(`  skip: ${file.wokwi} — local ${file.local} not present`);
                skipped++;
            } else {
                console.error(`  ERROR: ${file.wokwi} — local ${file.local} missing`);
                skipped++;
            }
            continue;
        }

        let localContent = fs.readFileSync(localPath, 'utf-8');
        if (file.adapt) localContent = adaptForWokwi(localContent, file.adapt);

        // Safety check on secrets.h.
        if (file.wokwi === 'secrets.h' && containsRealPat(localContent)) {
            console.error(`  REFUSED: ${file.wokwi} — ${file.local} contains a real-looking NOTEHUB_PAT`);
            skipped++;
            continue;
        }

        // Click the tab, read current content, compare.
        const clicked = await clickWokwiTab(page, file.wokwi);
        if (!clicked) {
            console.error(`  ERROR: could not find tab "${file.wokwi}" — is the filename right?`);
            skipped++;
            continue;
        }
        const wokwiContent = await getEditorContent(page);
        if (wokwiContent === null) {
            console.error(`  ERROR: could not read Monaco editor content`);
            skipped++;
            continue;
        }

        if (wokwiContent === localContent) {
            console.log(`  unchanged: ${file.wokwi}`);
            unchanged++;
            continue;
        }

        const localLines = localContent.split('\n').length;
        const wokwiLines = wokwiContent.split('\n').length;
        console.log(`  ${opts.dryRun ? 'WOULD UPDATE' : 'updating'}: ${file.wokwi}  (local: ${localLines} lines, wokwi: ${wokwiLines} lines)`);

        if (opts.dryRun) {
            changed++;
            continue;
        }

        await setEditorContent(page, localContent);
        await saveProject(page);
        console.log(`  saved: ${file.wokwi} (verifying against server after reload)`);
        changed++;
    }

    // Post-sync verify: reload the project page to bypass Monaco's in-memory
    // buffer, then re-read each just-updated file's content from what
    // Wokwi's server actually returns. Catches saves that were silently
    // dropped (e.g. by Wokwi's project-lock mechanism).
    if (changed > 0 && !opts.dryRun) {
        console.log(`  reloading to verify server-side content…`);
        await page.reload();
        await page.waitForSelector('.monaco-editor', { timeout: 30_000 });
        await page.waitForTimeout(2_000);

        for (const file of project.files) {
            const localPath = path.join(REPO_ROOT, file.local);
            if (!fs.existsSync(localPath)) continue;
            let localContent = fs.readFileSync(localPath, 'utf-8');
            if (file.adapt) localContent = adaptForWokwi(localContent, file.adapt);
            if (file.wokwi === 'secrets.h' && containsRealPat(localContent)) continue;

            await clickWokwiTab(page, file.wokwi);
            const serverContent = await getEditorContent(page);
            if (serverContent === localContent) {
                console.log(`    ✓ persisted: ${file.wokwi}`);
            } else {
                const serverLines = serverContent?.split('\n').length ?? '?';
                console.error(`    ✗ NOT PERSISTED: ${file.wokwi} (server: ${serverLines} lines, expected: ${localContent.split('\n').length} lines)`);
                verifyFailed++;
                changed--;
            }
        }
    }

    // Re-lock the project if we unlocked it.
    if (weUnlocked) {
        await relockProject(page);
    }

    await page.close();
    return { changed, unchanged, skipped, verifyFailed };
}

async function main() {
    const argv = process.argv.slice(2);
    const dryRun = argv.includes('--dry-run');
    const debug = argv.includes('--debug');
    const login = argv.includes('--login');
    const projectFilter = argv.find(a => !a.startsWith('--'));

    if (login) {
        await firstRunLogin();
        return;
    }

    // Prefer the persistent-profile flow (from --login on macOS with the
    // Chrome anti-detection args); fall back to storageState JSON if that's
    // all we have.
    let context;
    if (fs.existsSync(USER_DATA_DIR)) {
        context = await launchAuthedContext({ headless: !debug });
    } else if (fs.existsSync(AUTH_STATE_FILE)) {
        const browser = await chromium.launch({ headless: !debug });
        context = await browser.newContext({ storageState: AUTH_STATE_FILE });
    } else {
        console.error(`No auth found.\nRun:  node sync.js --login`);
        process.exit(1);
    }

    const totals = { changed: 0, unchanged: 0, skipped: 0, verifyFailed: 0 };
    for (const project of PROJECTS) {
        if (projectFilter && !project.name.includes(projectFilter)) continue;
        const s = await syncProject(context, project, { dryRun, debug });
        totals.changed += s.changed;
        totals.unchanged += s.unchanged;
        totals.skipped += s.skipped;
        totals.verifyFailed += s.verifyFailed;
    }

    await context.close();

    console.log(`\n━━━ done ━━━`);
    console.log(`  ${dryRun ? 'would change' : 'changed'}: ${totals.changed}`);
    console.log(`  unchanged:    ${totals.unchanged}`);
    console.log(`  skipped:      ${totals.skipped}`);
    if (totals.verifyFailed) {
        console.error(`  verify failed: ${totals.verifyFailed}`);
        process.exit(2);
    }
}

main().catch((err) => {
    console.error(err);
    process.exit(1);
});
