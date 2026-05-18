#ifndef ANTHROPIC_API_H
#define ANTHROPIC_API_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>
#include "config.h"
#include "OpenRouterAPI.h"  // for TokenData

class AnthropicAPI {
private:
    String apiKey;

    // Parse ISO-8601 UTC string → seconds until reset (or "now")
    String resetCountdown(const String& iso) {
        if (iso.isEmpty()) return "";
        time_t now = time(nullptr);
        if (now < 1000000000UL) return "";  // NTP not yet synced
        struct tm t = {};
        int y, mo, d, h, mi, s;
        if (sscanf(iso.c_str(), "%d-%d-%dT%d:%d:%dZ",
                   &y, &mo, &d, &h, &mi, &s) != 6) return "";
        t.tm_year = y - 1900; t.tm_mon = mo - 1; t.tm_mday = d;
        t.tm_hour = h; t.tm_min = mi; t.tm_sec = s;
        time_t reset_t = mktime(&t);
        long diff = (long)(reset_t - now);
        if (diff <= 0) return "now";
        int mins = (int)(diff / 60), secs = (int)(diff % 60);
        return (mins > 0) ? String(mins) + "m " + String(secs) + "s"
                          : String(secs) + "s";
    }

    int headerInt(HTTPClient& http, const char* name) {
        String v = http.header(name);
        return v.isEmpty() ? 0 : v.toInt();
    }

public:
    void setAPIKey(const char* key) { apiKey = String(key); }

    TokenData getRateLimits() {
        TokenData data;
        data.isAnthropicMode = true;

        if (WiFi.status() != WL_CONNECTED) { data.error = "WiFi off";  return data; }
        if (apiKey.isEmpty())              { data.error = "No API key"; return data; }

        WiFiClientSecure client;
        client.setInsecure();

        HTTPClient http;
        if (!http.begin(client, String(ANTHROPIC_BASE_URL) + "/v1/messages")) {
            data.error = "begin failed";
            return data;
        }

        http.addHeader("x-api-key",         apiKey);
        http.addHeader("anthropic-version",  ANTHROPIC_API_VERSION);
        http.addHeader("content-type",       "application/json");

        const char* hdrs[] = {
            "anthropic-ratelimit-requests-limit",
            "anthropic-ratelimit-requests-remaining",
            "anthropic-ratelimit-tokens-limit",
            "anthropic-ratelimit-tokens-remaining",
            "anthropic-ratelimit-tokens-reset",
            "anthropic-ratelimit-input-tokens-limit",
            "anthropic-ratelimit-input-tokens-remaining",
            "anthropic-ratelimit-output-tokens-limit",
            "anthropic-ratelimit-output-tokens-remaining",
        };
        http.collectHeaders(hdrs, 9);

        // Minimal request — just enough to get rate-limit headers
        String body = "{\"model\":\"" ANTHROPIC_MODEL "\",\"max_tokens\":1,"
                      "\"messages\":[{\"role\":\"user\",\"content\":\"1\"}]}";

        int code = http.POST(body);
        Serial.printf("[Anthropic] HTTP %d\n", code);

        if (code == 200 || code == 429) {
            data.req_limit     = headerInt(http, "anthropic-ratelimit-requests-limit");
            data.req_remaining = headerInt(http, "anthropic-ratelimit-requests-remaining");
            data.tok_limit     = headerInt(http, "anthropic-ratelimit-tokens-limit");
            data.tok_remaining = headerInt(http, "anthropic-ratelimit-tokens-remaining");
            data.in_limit      = headerInt(http, "anthropic-ratelimit-input-tokens-limit");
            data.in_remaining  = headerInt(http, "anthropic-ratelimit-input-tokens-remaining");
            data.out_limit     = headerInt(http, "anthropic-ratelimit-output-tokens-limit");
            data.out_remaining = headerInt(http, "anthropic-ratelimit-output-tokens-remaining");
            data.reset_in      = resetCountdown(http.header("anthropic-ratelimit-tokens-reset"));

            data.success = (data.tok_limit > 0 || data.req_limit > 0);
            if (!data.success) data.error = "No rate-limit headers";

            Serial.printf("[Anthropic] tok=%d/%d req=%d/%d reset=%s\n",
                          data.tok_remaining, data.tok_limit,
                          data.req_remaining, data.req_limit,
                          data.reset_in.c_str());
        } else if (code < 0) {
            data.error = "Net: " + String(http.errorToString(code));
        } else {
            data.error = "HTTP " + String(code);
        }

        http.end();
        return data;
    }
};

#endif // ANTHROPIC_API_H
