#ifndef OPENROUTER_API_H
#define OPENROUTER_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct TokenData {
    float usage;
    float limit;
    float credits;
    bool isLimited;
    String rateLimit;
    bool success;
    String error;
};

class OpenRouterAPI {
private:
    String apiKey;
    String apiUrl;

    // ── OpenRouter  ────────────────────────────────────────
    TokenData getOpenRouterCredits() {
        TokenData data = {0};
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
