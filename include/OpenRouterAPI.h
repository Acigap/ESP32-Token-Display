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
    
public:
    OpenRouterAPI(const char* url = "https://openrouter.ai/api/v1/auth/key") {
        apiUrl = url;
    }
    
    void setAPIKey(const char* key) {
        apiKey = key;
    }
    
    TokenData getCredits() {
        TokenData data = {0};
        
        if (WiFi.status() != WL_CONNECTED) {
            data.success = false;
            data.error = "WiFi not connected";
            return data;
        }
        
        if (apiKey.length() == 0) {
            data.success = false;
            data.error = "No API key set";
            return data;
        }
        
        HTTPClient http;
        http.begin(apiUrl);
        http.addHeader("Authorization", "Bearer " + apiKey);
        http.addHeader("Content-Type", "application/json");
        
        int httpResponseCode = http.GET();
        
        if (httpResponseCode == 200) {
            String payload = http.getString();
            
            // Parse JSON response
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            
            if (!error) {
                data.success = true;
                
                // Parse the data field
                JsonObject dataObj = doc["data"];
                
                if (dataObj.containsKey("usage")) {
                    data.usage = dataObj["usage"].as<float>();
                }
                
                if (dataObj.containsKey("limit")) {
                    data.limit = dataObj["limit"].as<float>();
                    data.isLimited = dataObj["is_free_tier"].as<bool>();
                }
                
                if (dataObj.containsKey("rate_limit")) {
                    JsonObject rateLimit = dataObj["rate_limit"];
                    if (rateLimit.containsKey("requests")) {
                        data.rateLimit = String(rateLimit["requests"].as<int>()) + " req";
                    }
                }
                
                // Calculate remaining credits
                if (data.limit > 0) {
                    data.credits = data.limit - data.usage;
                } else {
                    data.credits = -data.usage; // For unlimited accounts, show negative usage
                }
                
                Serial.println("Successfully fetched credits from OpenRouter");
                Serial.printf("Usage: %.2f, Limit: %.2f, Credits: %.2f\n", 
                             data.usage, data.limit, data.credits);
            } else {
                data.success = false;
                data.error = "JSON parse error: " + String(error.c_str());
                Serial.println(data.error);
            }
        } else {
            data.success = false;
            data.error = "HTTP Error: " + String(httpResponseCode);
            Serial.println(data.error);
            
            if (httpResponseCode > 0) {
                String response = http.getString();
                Serial.println("Response: " + response);
            }
        }
        
        http.end();
        return data;
    }
    
    String getAPIKey() {
        return apiKey;
    }
};

#endif // OPENROUTER_API_H
