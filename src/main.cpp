/**
 * XRP Price Ticker for Heltec WiFi LoRa 32 V3
 * 
 * Displays live XRP price from the XRPL DEX on the built-in OLED.
 * Configurable for different trading pairs (XRP/USD, XRP/RLUSD, XRP/SOLO, etc.)
 * 
 * Setup: Copy include/config.h.example to include/config.h and edit your settings.
 * 
 * Repository: https://github.com/justinnevins/xrp-ticker
 * License: MIT
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <SSD1306Wire.h>
#include "config.h"

using namespace websockets;

// Let's Encrypt ISRG Root X1 (valid until 2035)
const char* letsencrypt_root_ca = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// Display - initialized after hardware reset
SSD1306Wire *display = nullptr;

// WebSocket client
WebsocketsClient wsClient;

// State
bool wsConnected = false;
double xrpPrice = 0.0;
double bestBid = 0.0;
double bestAsk = 0.0;
unsigned long lastRequest = 0;
int requestId = 1;

void initDisplay() {
    // Hardware reset for OLED
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(50);
    
    // Initialize I2C
    Wire.begin(OLED_SDA, OLED_SCL);
    
    // Create display instance
    display = new SSD1306Wire(0x3c, OLED_SDA, OLED_SCL, GEOMETRY_128_64, I2C_ONE);
    display->init();
    display->flipScreenVertically();
    display->clear();
    display->display();
}

void showStatus(const char* line1, const char* line2 = "", const char* line3 = "") {
    display->clear();
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 0, line1);
    if (strlen(line2) > 0) display->drawString(0, 16, line2);
    if (strlen(line3) > 0) display->drawString(0, 32, line3);
    display->display();
}

void connectWiFi() {
    Serial.printf("Connecting to: %s\n", WIFI_SSID);
    showStatus("Connecting WiFi...", WIFI_SSID);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
        showStatus("WiFi: OK", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("\nFailed! Status: %d\n", WiFi.status());
        showStatus("WiFi: FAILED", "Check credentials");
    }
    delay(2000);
}

void connectWebSocket() {
    String wsUrl = "wss://" + String(XRPL_WS_HOST) + ":" + String(XRPL_WS_PORT) + XRPL_WS_PATH;
    Serial.printf("Connecting WS: %s\n", wsUrl.c_str());
    showStatus("Connecting XRPL...", XRPL_WS_HOST);
    
    wsClient.setCACert(letsencrypt_root_ca);
    wsConnected = wsClient.connect(wsUrl);
    
    Serial.printf("WS result: %s\n", wsConnected ? "OK" : "FAILED");
}

void requestOrderBook() {
    if (!wsConnected) return;
    
    // Request BID side: someone wants to buy XRP with QUOTE currency
    // TakerGets = XRP, TakerPays = QUOTE
    JsonDocument bidReq;
    bidReq["id"] = requestId++;
    bidReq["command"] = "book_offers";
    bidReq["taker_gets"]["currency"] = "XRP";
    bidReq["taker_pays"]["currency"] = QUOTE_CURRENCY;
    bidReq["taker_pays"]["issuer"] = QUOTE_ISSUER;
    bidReq["limit"] = 1;
    String bidJson;
    serializeJson(bidReq, bidJson);
    wsClient.send(bidJson);
    
    // Request ASK side: someone wants to sell XRP for QUOTE currency
    // TakerGets = QUOTE, TakerPays = XRP
    JsonDocument askReq;
    askReq["id"] = requestId++;
    askReq["command"] = "book_offers";
    askReq["taker_gets"]["currency"] = QUOTE_CURRENCY;
    askReq["taker_gets"]["issuer"] = QUOTE_ISSUER;
    askReq["taker_pays"]["currency"] = "XRP";
    askReq["limit"] = 1;
    String askJson;
    serializeJson(askReq, askJson);
    wsClient.send(askJson);
}

void onMessage(WebsocketsMessage msg) {
    JsonDocument doc;
    if (deserializeJson(doc, msg.data())) return;
    
    const char* status = doc["status"];
    if (!status || strcmp(status, "success") != 0) return;
    
    JsonArray offers = doc["result"]["offers"].as<JsonArray>();
    if (offers.size() == 0) return;
    
    JsonObject offer = offers[0];
    JsonVariant tg = offer["TakerGets"];
    
    // Determine which side this is based on TakerGets type
    if (tg.is<JsonObject>()) {
        // TakerGets is issued currency object - this is ASK side
        const char* quoteStr = tg["value"].as<const char*>();
        const char* dropsStr = offer["TakerPays"].as<const char*>();
        
        double quote = quoteStr ? atof(quoteStr) : 0;
        double drops = dropsStr ? atof(dropsStr) : 0;
        double xrp = drops / 1000000.0;
        
        if (quote > 0 && xrp > 0) {
            bestAsk = quote / xrp;
        }
    } else {
        // TakerGets is XRP drops as string - this is BID side
        const char* dropsStr = tg.as<const char*>();
        JsonVariant tp = offer["TakerPays"];
        const char* quoteStr = tp["value"].as<const char*>();
        
        double drops = dropsStr ? atof(dropsStr) : 0;
        double quote = quoteStr ? atof(quoteStr) : 0;
        double xrp = drops / 1000000.0;
        
        if (xrp > 0 && quote > 0) {
            bestBid = quote / xrp;
        }
    }
    
    // Calculate mid-price from bid/ask
    if (bestBid > 0 && bestAsk > 0 && bestBid < 10000 && bestAsk < 10000) {
        xrpPrice = (bestBid + bestAsk) / 2.0;
    } else if (bestBid > 0 && bestBid < 10000) {
        xrpPrice = bestBid;
    } else if (bestAsk > 0 && bestAsk < 10000) {
        xrpPrice = bestAsk;
    }
}

void onEvent(WebsocketsEvent event, String data) {
    if (event == WebsocketsEvent::ConnectionOpened) {
        wsConnected = true;
        Serial.println("WebSocket connected");
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        wsConnected = false;
        Serial.println("WebSocket disconnected");
    }
}

void updateDisplay() {
    display->clear();
    
    if (xrpPrice > 0 && xrpPrice < 10000 && !isinf(xrpPrice)) {
        // Build price string with configured symbol and decimals
        char price[24];
        char format[16];
        snprintf(format, sizeof(format), "%s%%.%df", QUOTE_SYMBOL, QUOTE_DECIMALS);
        snprintf(price, sizeof(price), format, xrpPrice);
        
        // Header: pair label + connection status
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 0, DISPLAY_LABEL);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 0, wsConnected ? "LIVE" : "...");
        
        // Main price - large font
        display->setFont(ArialMT_Plain_24);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 18, price);
        
        // Bid/Ask spread at bottom
        display->setFont(ArialMT_Plain_10);
        char bid[16], ask[16];
        snprintf(format, sizeof(format), "%%.%df", QUOTE_DECIMALS);
        snprintf(bid, sizeof(bid), format, bestBid);
        snprintf(ask, sizeof(ask), format, bestAsk);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 54, bid);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 54, ask);
    } else {
        // Loading/connecting state
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 8, DISPLAY_LABEL);
        display->setFont(ArialMT_Plain_10);
        display->drawString(64, 32, wsConnected ? "Fetching price..." : "Connecting...");
    }
    
    display->display();
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== XRP Price Ticker ===");
    Serial.printf("Pair: %s\n", DISPLAY_LABEL);
    Serial.printf("Update interval: %dms\n", PRICE_UPDATE_INTERVAL);
    
    initDisplay();
    showStatus("XRP Price Ticker", DISPLAY_LABEL, "Initializing...");
    delay(1000);
    
    connectWiFi();
    
    wsClient.onMessage(onMessage);
    wsClient.onEvent(onEvent);
    connectWebSocket();
}

void loop() {
    if (wsConnected) {
        wsClient.poll();
        if (millis() - lastRequest >= PRICE_UPDATE_INTERVAL) {
            requestOrderBook();
            lastRequest = millis();
        }
    } else {
        // Reconnect if disconnected
        connectWebSocket();
        delay(5000);
    }
    
    updateDisplay();
    delay(50);
}
