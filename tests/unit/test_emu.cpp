// Unit tests for note-emu core (note/emu/emu.c)
//
// Uses a mock http_post callback to test all code paths without network access.

#define CATCH_CONFIG_MAIN
#include "catch.hpp"

extern "C" {
#include <note/emu/emu.h>
}

#include <cstring>
#include <string>
#include <vector>
#include <functional>

// ═══════════════════════════════════════════════════════════════════════
// Mock HTTP backend
// ═══════════════════════════════════════════════════════════════════════

struct HttpCall {
    std::string url;
    std::string body;
    std::vector<std::string> headers;
};

struct MockHttp {
    // What the mock returns
    int status = 200;
    std::string response;
    int transport_rc = 0;  // 0 = success, non-zero = transport error

    // Recorded calls
    std::vector<HttpCall> calls;

    // Optional: per-call behavior (overrides status/response/transport_rc)
    std::function<int(const HttpCall &, int *status, std::string &response)> on_call;

    void reset() {
        status = 200;
        response.clear();
        transport_rc = 0;
        calls.clear();
        on_call = nullptr;
    }
};

static MockHttp g_mock;

static int mock_http_post(
    const char *url, const char *const *headers,
    const uint8_t *body, size_t body_len,
    uint8_t *response_buf, size_t response_buf_size,
    size_t *response_len, int *http_status, void *ctx
) {
    (void)ctx;
    HttpCall call;
    call.url = url;
    if (body && body_len > 0)
        call.body.assign(reinterpret_cast<const char *>(body), body_len);
    for (const char *const *h = headers; h && *h; h++)
        call.headers.emplace_back(*h);
    g_mock.calls.push_back(call);

    int rc = g_mock.transport_rc;
    int st = g_mock.status;
    std::string resp = g_mock.response;

    if (g_mock.on_call) {
        rc = g_mock.on_call(call, &st, resp);
    }

    *http_status = st;
    *response_len = 0;
    if (rc == 0 && st > 0 && !resp.empty()) {
        size_t n = resp.size();
        if (n > response_buf_size) n = response_buf_size;
        memcpy(response_buf, resp.data(), n);
        *response_len = n;
    }
    return rc;
}

static uint32_t mock_millis(void *ctx) {
    (void)ctx;
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════
// Helpers
// ═══════════════════════════════════════════════════════════════════════

static const char *TEST_TOKEN = "test-pat-token";
static const char *TEST_UID = "abc-123-def-456";

// A billing-accounts response containing a uid field
static std::string uid_response(const char *uid) {
    return std::string(R"({"billing_account_uid":"x","uid":")") + uid + R"("})";
}

// Create a config with the mock backend and a PAT token
static note_emu_config_t make_config() {
    note_emu_config_t config{};
    config.http_post = mock_http_post;
    config.millis = mock_millis;
    config.api_token = TEST_TOKEN;
    config.disable_logging = true;
    return config;
}

// Create a note-emu instance (helper for tests that need a ready instance)
static note_emu_t *create_instance() {
    g_mock.reset();
    g_mock.response = uid_response(TEST_UID);
    auto config = make_config();
    note_emu_t *emu = nullptr;
    REQUIRE(note_emu_create(&config, &emu) == NOTE_EMU_OK);
    REQUIRE(emu != nullptr);
    g_mock.calls.clear();  // clear the UID resolution call
    return emu;
}

// ═══════════════════════════════════════════════════════════════════════
// Happy path tests
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("create with PAT resolves UID", "[create][happy]") {
    g_mock.reset();
    g_mock.response = uid_response(TEST_UID);

    auto config = make_config();
    note_emu_t *emu = nullptr;
    REQUIRE(note_emu_create(&config, &emu) == NOTE_EMU_OK);
    REQUIRE(emu != nullptr);

    // Should have made one HTTP call to billing-accounts
    REQUIRE(g_mock.calls.size() == 1);
    CHECK(g_mock.calls[0].url == "https://api.notefile.net/v1/billing-accounts");
    // Body should be NULL (GET request)
    CHECK(g_mock.calls[0].body.empty());

    note_emu_destroy(emu);
}

TEST_CASE("create with pre-set UID skips resolution", "[create][happy]") {
    g_mock.reset();

    auto config = make_config();
    config.user_uid = TEST_UID;
    config.api_token = nullptr;  // not needed when UID is provided

    note_emu_t *emu = nullptr;
    REQUIRE(note_emu_create(&config, &emu) == NOTE_EMU_OK);
    REQUIRE(emu != nullptr);

    // No HTTP calls — UID was provided directly
    CHECK(g_mock.calls.empty());

    note_emu_destroy(emu);
}

TEST_CASE("write and read round-trip", "[write][read][happy]") {
    auto *emu = create_instance();

    // Write some data
    g_mock.reset();
    g_mock.status = 200;
    const uint8_t data[] = "hello";
    CHECK(note_emu_write(emu, data, sizeof(data)) == NOTE_EMU_OK);
    REQUIRE(g_mock.calls.size() == 1);
    CHECK(g_mock.calls[0].url.find("/v1/write") != std::string::npos);

    // Read response
    g_mock.reset();
    g_mock.status = 200;
    g_mock.response = "world";
    uint8_t buf[64] = {};
    int n = note_emu_read(emu, buf, sizeof(buf));
    CHECK(n == 5);
    CHECK(std::string(reinterpret_cast<char *>(buf), n) == "world");
    REQUIRE(g_mock.calls.size() == 1);
    CHECK(g_mock.calls[0].url.find("/v1/read") != std::string::npos);

    note_emu_destroy(emu);
}

TEST_CASE("read returns buffered data without HTTP call", "[read][happy]") {
    auto *emu = create_instance();

    // First read fetches from HTTP
    g_mock.reset();
    g_mock.response = "abcdef";
    uint8_t buf[3] = {};
    int n = note_emu_read(emu, buf, 3);  // only read 3 of 6 bytes
    CHECK(n == 3);
    CHECK(g_mock.calls.size() == 1);

    // Second read serves from buffer — no HTTP call
    g_mock.calls.clear();
    n = note_emu_read(emu, buf, 3);
    CHECK(n == 3);
    CHECK(g_mock.calls.empty());
    CHECK(std::string(reinterpret_cast<char *>(buf), n) == "def");

    note_emu_destroy(emu);
}

TEST_CASE("read returns 0 for empty response", "[read][happy]") {
    auto *emu = create_instance();

    g_mock.reset();
    g_mock.response = "";
    uint8_t buf[64] = {};
    int n = note_emu_read(emu, buf, sizeof(buf));
    CHECK(n == 0);

    note_emu_destroy(emu);
}

TEST_CASE("serial handshake returns \\r\\n locally", "[serial][happy]") {
    auto *emu = create_instance();
    note_emu_set_global(emu);
    g_mock.calls.clear();

    // Send bare \n (note-c reset handshake)
    uint8_t newline[] = {'\n'};
    note_emu_serial_transmit(newline, 1, true);

    // Should NOT have made an HTTP call
    CHECK(g_mock.calls.empty());

    // Should have \r\n available
    CHECK(note_emu_serial_available() == true);
    CHECK(note_emu_serial_receive() == '\r');
    CHECK(note_emu_serial_receive() == '\n');

    note_emu_set_global(nullptr);
    note_emu_destroy(emu);
}

TEST_CASE("serial transmit sends request on newline", "[serial][happy]") {
    auto *emu = create_instance();
    note_emu_set_global(emu);
    g_mock.reset();
    g_mock.status = 200;

    // Send a JSON request (more than just \n)
    const char *req = "{\"req\":\"card.version\"}\n";
    note_emu_serial_transmit(
        reinterpret_cast<uint8_t *>(const_cast<char *>(req)),
        strlen(req), false);

    // Should have made an HTTP call to /v1/write
    REQUIRE(g_mock.calls.size() == 1);
    CHECK(g_mock.calls[0].url.find("/v1/write") != std::string::npos);

    note_emu_set_global(nullptr);
    note_emu_destroy(emu);
}

TEST_CASE("serial available returns false when no write pending", "[serial][happy]") {
    auto *emu = create_instance();
    note_emu_set_global(emu);
    g_mock.calls.clear();

    // No write has been done — available should return false without HTTP
    CHECK(note_emu_serial_available() == false);
    CHECK(g_mock.calls.empty());

    note_emu_set_global(nullptr);
    note_emu_destroy(emu);
}

TEST_CASE("serial awaiting_response set by write, cleared by read", "[serial][happy]") {
    auto *emu = create_instance();
    note_emu_set_global(emu);

    // available() returns false before any write
    g_mock.reset();
    CHECK(note_emu_serial_available() == false);

    // Write a request — sets awaiting_response
    g_mock.status = 200;
    const char *req = "{\"req\":\"test\"}\n";
    note_emu_serial_transmit(
        reinterpret_cast<uint8_t *>(const_cast<char *>(req)),
        strlen(req), false);
    g_mock.calls.clear();

    // Now available() should poll the network
    g_mock.response = "{}\r\n";
    CHECK(note_emu_serial_available() == true);
    REQUIRE(g_mock.calls.size() == 1);  // made an HTTP call

    // Drain the response
    while (note_emu_serial_available()) {
        note_emu_serial_receive();
    }

    // After draining, available() should not poll again (awaiting_response cleared)
    g_mock.calls.clear();
    CHECK(note_emu_serial_available() == false);
    CHECK(g_mock.calls.empty());

    note_emu_set_global(nullptr);
    note_emu_destroy(emu);
}

// ═══════════════════════════════════════════════════════════════════════
// Error path tests — create
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("create fails with NULL config", "[create][error]") {
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(nullptr, &emu) == NOTE_EMU_ERR_INVALID_ARG);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails with NULL emu_out", "[create][error]") {
    auto config = make_config();
    CHECK(note_emu_create(&config, nullptr) == NOTE_EMU_ERR_INVALID_ARG);
}

TEST_CASE("create fails without http_post", "[create][error]") {
    auto config = make_config();
    config.http_post = nullptr;
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_INVALID_CONFIG);
}

TEST_CASE("create fails without millis", "[create][error]") {
    auto config = make_config();
    config.millis = nullptr;
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_INVALID_CONFIG);
}

TEST_CASE("create fails without token or UID", "[create][error]") {
    auto config = make_config();
    config.api_token = nullptr;
    config.user_uid = nullptr;
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_INVALID_CONFIG);
}

// ═══════════════════════════════════════════════════════════════════════
// Error path tests — PAT / UID resolution
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("create fails with invalid PAT (401)", "[create][auth][error]") {
    g_mock.reset();
    g_mock.status = 401;

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_AUTH);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails with expired PAT (403)", "[create][auth][error]") {
    g_mock.reset();
    g_mock.status = 403;

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_AUTH);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails on transport error during UID resolution", "[create][error]") {
    g_mock.reset();
    g_mock.transport_rc = -1;

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_HTTP);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails on server error (500) during UID resolution", "[create][error]") {
    g_mock.reset();
    g_mock.status = 500;

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_HTTP);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails on empty response from billing-accounts", "[create][error]") {
    g_mock.reset();
    g_mock.response = "";

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_PARSE);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails when response has no uid field", "[create][error]") {
    g_mock.reset();
    g_mock.response = R"({"name":"test account"})";

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_PARSE);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails when uid value is malformed (no closing quote)", "[create][error]") {
    g_mock.reset();
    g_mock.response = R"({"uid":"abc-123-no-close)";

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_PARSE);
    CHECK(emu == nullptr);
}

TEST_CASE("create fails when uid is too long", "[create][error]") {
    g_mock.reset();
    // 64+ character UID exceeds the resolved_uid buffer
    std::string long_uid(100, 'x');
    g_mock.response = R"({"uid":")" + long_uid + R"("})";

    auto config = make_config();
    note_emu_t *emu = nullptr;
    CHECK(note_emu_create(&config, &emu) == NOTE_EMU_ERR_PARSE);
    CHECK(emu == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
// Error path tests — write
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("write fails with NULL emu", "[write][error]") {
    const uint8_t data[] = "test";
    CHECK(note_emu_write(nullptr, data, sizeof(data)) == NOTE_EMU_ERR_INVALID_ARG);
}

TEST_CASE("write fails with NULL data", "[write][error]") {
    auto *emu = create_instance();
    CHECK(note_emu_write(emu, nullptr, 5) == NOTE_EMU_ERR_INVALID_ARG);
    note_emu_destroy(emu);
}

TEST_CASE("write fails with zero length", "[write][error]") {
    auto *emu = create_instance();
    const uint8_t data[] = "test";
    CHECK(note_emu_write(emu, data, 0) == NOTE_EMU_ERR_INVALID_ARG);
    note_emu_destroy(emu);
}

TEST_CASE("write fails on transport error", "[write][error]") {
    auto *emu = create_instance();
    g_mock.reset();
    g_mock.transport_rc = -1;

    const uint8_t data[] = "test";
    CHECK(note_emu_write(emu, data, sizeof(data)) == NOTE_EMU_ERR_HTTP);

    note_emu_destroy(emu);
}

TEST_CASE("write retries on 401 and succeeds", "[write][auth][happy]") {
    auto *emu = create_instance();

    int call_count = 0;
    g_mock.reset();
    g_mock.on_call = [&](const HttpCall &, int *st, std::string &) -> int {
        call_count++;
        // First call to /v1/write returns 401, retry succeeds
        if (call_count == 1) {
            *st = 401;
        } else {
            *st = 200;
        }
        return 0;
    };

    const uint8_t data[] = "test";
    CHECK(note_emu_write(emu, data, sizeof(data)) == NOTE_EMU_OK);
    // Should have called http_post twice (original + retry)
    CHECK(call_count == 2);

    note_emu_destroy(emu);
}

TEST_CASE("write fails on non-2xx status", "[write][error]") {
    auto *emu = create_instance();
    g_mock.reset();
    g_mock.status = 500;

    const uint8_t data[] = "test";
    CHECK(note_emu_write(emu, data, sizeof(data)) == NOTE_EMU_ERR_HTTP);

    note_emu_destroy(emu);
}

// ═══════════════════════════════════════════════════════════════════════
// Error path tests — read
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("read fails with NULL emu", "[read][error]") {
    uint8_t buf[64];
    CHECK(note_emu_read(nullptr, buf, sizeof(buf)) == NOTE_EMU_ERR_INVALID_ARG);
}

TEST_CASE("read fails with NULL buffer", "[read][error]") {
    auto *emu = create_instance();
    CHECK(note_emu_read(emu, nullptr, 64) == NOTE_EMU_ERR_INVALID_ARG);
    note_emu_destroy(emu);
}

TEST_CASE("read fails on transport error", "[read][error]") {
    auto *emu = create_instance();
    g_mock.reset();
    g_mock.transport_rc = -1;

    uint8_t buf[64];
    CHECK(note_emu_read(emu, buf, sizeof(buf)) == NOTE_EMU_ERR_HTTP);

    note_emu_destroy(emu);
}

TEST_CASE("read retries on 401 and succeeds", "[read][auth][happy]") {
    auto *emu = create_instance();

    int call_count = 0;
    g_mock.reset();
    g_mock.on_call = [&](const HttpCall &, int *st, std::string &resp) -> int {
        call_count++;
        if (call_count == 1) {
            *st = 401;
        } else {
            *st = 200;
            resp = "response-data";
        }
        return 0;
    };

    uint8_t buf[64] = {};
    int n = note_emu_read(emu, buf, sizeof(buf));
    CHECK(n == 13);
    CHECK(std::string(reinterpret_cast<char *>(buf), n) == "response-data");
    CHECK(call_count == 2);

    note_emu_destroy(emu);
}

TEST_CASE("serial reset clears state", "[serial][happy]") {
    auto *emu = create_instance();
    note_emu_set_global(emu);

    CHECK(note_emu_serial_reset() == true);
    CHECK(note_emu_serial_available() == false);
    CHECK(note_emu_serial_receive() == '\0');

    note_emu_set_global(nullptr);
    note_emu_destroy(emu);
}

TEST_CASE("serial functions return safely with no global", "[serial][error]") {
    note_emu_set_global(nullptr);

    CHECK(note_emu_serial_reset() == false);
    CHECK(note_emu_serial_available() == false);
    CHECK(note_emu_serial_receive() == '\0');

    // transmit should not crash
    uint8_t data[] = "test";
    note_emu_serial_transmit(data, sizeof(data), true);
}
