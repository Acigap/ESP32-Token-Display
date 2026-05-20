#ifndef RELAY_API_H
#define RELAY_API_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

struct RelayData {
    bool    ok           = false;
    float   sessionPct   = 0.0f;
    String  sessionReset = "?";
    float   weeklyPct    = 0.0f;
    String  weeklyReset  = "?";
    String  error;
};

class RelayAPI {
    String _url;
public:
    void setUrl(const char* host, int port) {
        _url = String("http://") + host + ":" + String(port) + "/usage";
    }

    RelayData fetch() {
        RelayData d;
        if (WiFi.status() != WL_CONNECTED || _url.isEmpty()) {
            d.error = "offline";
            return d;
        }
        HTTPClient http;
        http.begin(_url);
        http.setTimeout(3000);
        int code = http.GET();
        if (code == 200) {
            JsonDocument doc;
            if (!deserializeJson(doc, http.getString())) {
                d.ok           = doc["ok"]          | false;
                d.sessionPct   = doc["session_pct"] | 0.0f;
                d.weeklyPct    = doc["weekly_pct"]  | 0.0f;
                if (const char* sr = doc["session_resets_in"]) d.sessionReset = sr;
                if (const char* wr = doc["weekly_resets_in"])  d.weeklyReset  = wr;
            } else {
                d.error = "JSON err";
            }
        } else {
            d.error = code < 0 ? "offline" : ("HTTP " + String(code));
        }
        http.end();
        return d;
    }
};

#endif // RELAY_API_H
