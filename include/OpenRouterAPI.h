#ifndef OPENROUTER_API_H
#define OPENROUTER_API_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "config.h"

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

    // ── OpenAI (legacy billing API — still works for pay-as-you-go) ──
    TokenData getOpenAICredits() {
        TokenData data = {0};
        if (WiFi.status() != WL_CONNECTED) { data.error = "WiFi not connected"; return data; }
        if (apiKey.length() == 0)          { data.error = "No API key set";      return data; }

        // 1) Subscription → hard_limit_usd
        HTTPClient http;
        http.begin("https://api.openai.com/dashboard/billing/subscription");
        http.addHeader("Authorization", "Bearer " + apiKey);
        int code = http.GET();
        if (code == 200) {
            JsonDocument doc;
            if (!deserializeJson(doc, http.getString()))
                data.limit = doc["hard_limit_usd"] | 0.0f;
        }
        http.end();

        // 2) Usage this month → total_usage (cents)
        time_t now = time(nullptr);
        if (now > 1000000UL) {   // NTP synced
            struct tm* t = localtime(&now);
            char sd[12], ed[12];
            strftime(sd, sizeof(sd), "%Y-%m-01", t);
            strftime(ed, sizeof(ed), "%Y-%m-%d", t);
            String url = "https://api.openai.com/dashboard/billing/usage?start_date=";
            url += sd; url += "&end_date="; url += ed;

            http.begin(url);
            http.addHeader("Authorization", "Bearer " + apiKey);
            code = http.GET();
            if (code == 200) {
                JsonDocument doc;
                if (!deserializeJson(doc, http.getString()))
                    data.usage = (doc["total_usage"] | 0.0f) / 100.0f;  // cents → $
            }
            http.end();
        }

        data.credits = data.limit - data.usage;
        data.success = (data.limit > 0 || data.usage > 0);
        if (!data.success) data.error = "No billing data (check key permissions)";
        Serial.printf("[OAI] Usage: $%.4f  Limit: $%.2f\n", data.usage, data.limit);
        return data;
    }

public:
    OpenRouterAPI(const char* url = "https://openrouter.ai/api/v1/auth/key") {
        apiUrl = url;
    }

    void setAPIKey(const char* key) { apiKey = key; }

    // Dispatch to the correct provider's API
    TokenData getCredits(ProviderType provider = PROVIDER_OPENROUTER) {
        switch (provider) {
            case PROVIDER_OPENAI: return getOpenAICredits();
            default:              return getOpenRouterCredits();
        }
    }

    String getAPIKey() { return apiKey; }
};

#endif // OPENROUTER_API_H
