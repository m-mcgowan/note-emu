// note-emu native demo / integration test
//
// Exercises the softcard protocol from a desktop machine using libcurl.
//
// Environment variables:
//   NOTEHUB_PAT          — Notehub Personal Access Token (required)
//   NOTEHUB_PRODUCT_UID  — Notehub product UID (required for "project" mode)
//   SOFTCARD_URL         — Override service URL (optional)
//
// Usage:
//   ./note-emu-demo              Basic test: card.version, card.temp, card.status
//   ./note-emu-demo project      Project test: hub.set, note.add, hub.get

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "note/emu/emu.h"
#include "note/emu/curl.h"

// Send a JSON request, print it, read response into buf.
// Returns response length on success, negative on error.
static int transact(note_emu_t *emu, const char *request, char *resp, size_t resp_size) {
    printf("  >> %s", request);

    int rc = note_emu_write(emu, (const uint8_t *)request, strlen(request));
    if (rc != 0) {
        printf("  !! write failed: %d\n", rc);
        return -1;
    }

    int n = note_emu_read(emu, (uint8_t *)resp, resp_size - 1);
    if (n < 0) {
        printf("  !! read failed: %d\n", n);
        return n;
    }
    if (n == 0) {
        printf("  !! empty response\n");
        return -1;
    }

    resp[n] = '\0';
    printf("  << %s", resp);
    if (resp[n - 1] != '\n') printf("\n");

    return n;
}

// Check if response contains an "err" field (simple string scan).
static int has_error(const char *resp) {
    return strstr(resp, "\"err\"") != NULL;
}

// Extract a string value for a key from JSON (simple, no nesting).
// Writes into out (up to out_size-1 chars). Returns 0 on success.
static int extract_string(const char *json, const char *key, char *out, size_t out_size) {
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\":\"", key);
    const char *start = strstr(json, needle);
    if (!start) return -1;
    start += strlen(needle);
    const char *end = strchr(start, '"');
    if (!end) return -1;
    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 0;
}

// ── Test: basic (no project) ─────────────────────────────────────────

static int test_basic(note_emu_t *emu) {
    printf("Test: basic\n\n");
    char resp[2048];
    int failures = 0;

    if (transact(emu, "{\"req\":\"card.version\"}\n", resp, sizeof(resp)) < 0)
        failures++;
    if (transact(emu, "{\"req\":\"card.temp\"}\n", resp, sizeof(resp)) < 0)
        failures++;
    if (transact(emu, "{\"req\":\"card.status\"}\n", resp, sizeof(resp)) < 0)
        failures++;

    printf("\n%s (%d/%d requests succeeded)\n",
           failures == 0 ? "PASS" : "FAIL", 3 - failures, 3);
    return failures;
}

// ── Test: project (hub.set + note.add + hub.get) ─────────────────────

static int test_project(note_emu_t *emu) {
    const char *product = getenv("NOTEHUB_PRODUCT_UID");
    if (!product || !product[0]) {
        fprintf(stderr, "error: NOTEHUB_PRODUCT_UID environment variable is required for project test\n");
        return 1;
    }

    printf("Test: project (%s)\n\n", product);
    char resp[2048];
    char req[512];
    int failures = 0;

    // 1. hub.set — assign project
    snprintf(req, sizeof(req),
             "{\"req\":\"hub.set\",\"product\":\"%s\",\"mode\":\"minimum\"}\n",
             product);
    if (transact(emu, req, resp, sizeof(resp)) < 0 || has_error(resp)) {
        printf("  !! hub.set failed\n");
        failures++;
    }

    // 2. note.add — send a test note with unique marker
    char test_id[32];
    snprintf(test_id, sizeof(test_id), "test_%ld", (long)time(NULL));
    long timestamp = (long)time(NULL);

    snprintf(req, sizeof(req),
             "{\"req\":\"note.add\",\"file\":\"test.qo\","
             "\"body\":{\"test_id\":\"%s\",\"timestamp\":%ld},"
             "\"sync\":true}\n",
             test_id, timestamp);
    if (transact(emu, req, resp, sizeof(resp)) < 0 || has_error(resp)) {
        printf("  !! note.add failed\n");
        failures++;
    }

    // 3. hub.get — confirm project is set, get device UID
    if (transact(emu, "{\"req\":\"hub.get\"}\n", resp, sizeof(resp)) < 0) {
        printf("  !! hub.get failed\n");
        failures++;
    }

    char device[80] = "";
    extract_string(resp, "device", device, sizeof(device));

    printf("\n%s (%d/%d requests succeeded)\n",
           failures == 0 ? "PASS" : "FAIL", 3 - failures, 3);

    // Machine-readable summary for Python verifier
    printf("RESULT: device=%s test_id=%s product=%s\n", device, test_id, product);

    return failures;
}

// ── Main ─────────────────────────────────────────────────────────────

int main(int argc, char *argv[]) {
    const char *token = getenv("NOTEHUB_PAT");
    if (!token || !token[0]) {
        fprintf(stderr, "error: NOTEHUB_PAT environment variable is required\n");
        return 1;
    }

    const char *mode = (argc > 1) ? argv[1] : "basic";
    const char *url = getenv("SOFTCARD_URL");

    printf("note-emu demo\n\n");

    if (note_emu_curl_init() != 0) {
        fprintf(stderr, "error: curl_global_init failed\n");
        return 1;
    }

    printf("Resolving account UID from token...\n");

    note_emu_config_t config = {
        .http_post   = note_emu_curl_post,
        .millis      = note_emu_posix_millis,
        .ctx         = NULL,
        .api_token   = token,
        .service_url = url,
    };

    note_emu_t *emu;
    note_emu_err_t err = note_emu_create(&config, &emu);
    if (err != NOTE_EMU_OK) {
        fprintf(stderr, "error: %s\n", note_emu_strerror(err));
        note_emu_curl_cleanup();
        return 1;
    }

    printf("Connected to %s\n\n", url ? url : NOTE_EMU_DEFAULT_URL);

    int rc;
    if (strcmp(mode, "project") == 0) {
        rc = test_project(emu);
    } else {
        rc = test_basic(emu);
    }

    note_emu_destroy(emu);
    note_emu_curl_cleanup();
    return rc ? 1 : 0;
}
