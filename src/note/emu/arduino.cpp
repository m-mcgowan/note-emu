// note::emu::Arduino — implementation

#include "arduino.hpp"

#ifdef ARDUINO

#include <Arduino.h>

namespace note::emu {

Arduino::Arduino(const char *api_token)
    : _client(nullptr)
    , _log_output(&Serial)
    , _config{}
    , _emu(nullptr)
    , _http_initialized(false)
{
    _config.http_post = httpPost;
    _config.millis = getMillis;
    _config.ctx = this;
    _config.api_token = api_token;
    _config.log_fn = logToPrint;
}

note_emu_err_t Arduino::begin(NetworkClient &client) {
    _client = &client;
    return note_emu_create(&_config, &_emu);
}

void Arduino::setDebugOutput(Print &output) {
    _log_output = &output;
    _config.log_fn = logToPrint;
    _config.disable_logging = false;
}

void Arduino::disableLogging() {
    _log_output = nullptr;
    _config.disable_logging = true;
}

void Arduino::logToPrint(const char *msg, void *ctx) {
    auto &self = *static_cast<Arduino *>(ctx);
    if (self._log_output) {
        self._log_output->print("note-emu: ");
        self._log_output->println(msg);
    }
}

int Arduino::httpPost(
    const char *url, const char *const *headers,
    const uint8_t *body, size_t body_len,
    uint8_t *response_buf, size_t response_buf_size,
    size_t *response_len, int *http_status, void *ctx
) {
    auto &self = *static_cast<Arduino *>(ctx);

    if (!self._http_initialized) {
        self._http.setReuse(true);
        self._http.setTimeout(30000);
        self._http_initialized = true;
    }

    // Finalize previous request. When keep-alive is active, this preserves
    // the TCP connection so begin() can reuse it for same-host requests.
    self._http.end();
    self._http.begin(*self._client, url);

    for (const char *const *h = headers; h && *h; h++) {
        const char *colon = strchr(*h, ':');
        if (!colon) continue;
        String key = String(*h).substring(0, colon - *h);
        String val = String(colon + 2);
        self._http.addHeader(key, val);
    }

    int status;
    if (body && body_len > 0) {
        self._http.addHeader("Content-Type", "application/octet-stream");
        status = self._http.POST(const_cast<uint8_t *>(body), body_len);
    } else if (body) {
        status = self._http.POST("");
    } else {
        status = self._http.GET();
    }

    *http_status = status;
    *response_len = 0;

    if (status > 0) {
        String payload = self._http.getString();
        size_t n = payload.length();
        if (n > response_buf_size) n = response_buf_size;
        memcpy(response_buf, payload.c_str(), n);
        *response_len = n;
    }

    return (status > 0) ? 0 : -1;
}

uint32_t Arduino::getMillis(void *ctx) {
    (void)ctx;
    return ::millis();
}

}  // namespace note::emu

#endif // ARDUINO
