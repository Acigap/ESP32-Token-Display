#ifndef OPENROUTER_API_H
#define OPENROUTER_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct TokenData {
    // ── OpenRouter mode ──────────────────────────────────
    float  usage    = 0;
    float  limit    = 0;
    float  credits  = 0;
    bool   isLimited = false;
    String rateLimit;

    // ── Anthropic rate-limit mode ────────────────────────
    bool   isAnthropicMode = false;
    int    req_limit     = 0;
    int    req_remaining = 0;
    int    tok_limit     = 0;
    int    tok_remaining = 0;
    int    in_limit      = 0;
    int    in_remaining  = 0;
    int    out_limit     = 0;
    int    out_remaining = 0;
    String reset_in;            // "45s" or "1m 23s"

    // ── Common ───────────────────────────────────────────
    bool   success = false;
    String error;
};

class OpenRouterAPI {
private:
    String apiKey;
    String apiUrl;

    // ── OpenRouter  ────────────────────────────────────────
    TokenData getOpenRouterCredits() {
        TokenData data;
        if (WiFi.status() != WL_CONNECTED) { data.error = "WiFi not connected"; return data; }
        if (apiKey.length() == 0)          { data.error = "No API key set";      return data; }

        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Authorization", "Bearer " + apiKey);
        http.addHeader("Content-Type", "application/json");
        int code = http.GET();

        if (code == 200) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, http.getString());
            if (!err) {
                data.success = true;
                JsonObject d = doc["data"];
                if (d["usage"].is<float>())      data.usage  = d["usage"].as<float>();
                if (d["limit"].is<float>())      { data.limit = d["limit"].as<float>(); data.isLimited = true; }
                if (d["rate_limit"].is<JsonObject>()) {
                    JsonObject rl = d["rate_limit"];
                    if (rl["requests"].is<int>())
                        data.rateLimit = String(rl["requests"].as<int>()) + " req";
                }
                data.credits = (data.limit > 0) ? (data.limit - data.usage) : -data.usage;
                Serial.printf("[OR] Usage: %.4f  Limit: %.2f\n", data.usage, data.limit);
            } else {
                data.error = "JSON parse error: " + String(err.c_str());
            }
        } else {
            data.error = "HTTP " + String(code);
        }
        http.end();
        return data;
    }

public:
    OpenRouterAPI(const char* url = "https://openrouter.ai/api/v1/auth/key") {
        apiUrl = url;
    }

    void setAPIKey(const char* key) { apiKey = key; }

    TokenData getCredits() { return getOpenRouterCredits(); }

    String getAPIKey() { return apiKey; }
};

#endif // OPENROUTER_API_H
