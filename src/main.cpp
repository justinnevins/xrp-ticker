/**
 * XRP Price Ticker + Portfolio Tracker for Heltec WiFi LoRa 32 V3
 * 
 * Press the PRG button to cycle between:
 *   - Live price ticker (default)
 *   - Portfolio value
 * 
 * Setup: Copy include/config.h.example to include/config.h and edit your settings.
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

// User button (PRG button on Heltec V3)
#define BUTTON_PIN 0

// Display modes
#define MODE_TICKER 0
#define MODE_PORTFOLIO 1
int displayMode = MODE_TICKER;

// Let's Encrypt ISRG Root X1
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

// Display
SSD1306Wire *display = nullptr;

// WebSocket
WebsocketsClient wsClient;
bool wsConnected = false;

// Ticker state
double xrpPrice = 0.0;
double bestBid = 0.0;
double bestAsk = 0.0;
unsigned long lastPriceRequest = 0;
int requestId = 1;

// Price alert state
#define PRICE_HISTORY_SIZE 60
double priceHistory[PRICE_HISTORY_SIZE];
unsigned long priceTimestamps[PRICE_HISTORY_SIZE];
int priceHistoryIndex = 0;
int priceHistoryCount = 0;
bool alertActive = false;
unsigned long alertStartTime = 0;
double percentChange = 0.0;

// Portfolio state
#ifdef WALLET_COUNT
double walletBalances[WALLET_COUNT] = {0};
int balanceRequestIds[WALLET_COUNT] = {0};
#else
double walletBalances[1] = {0};
int balanceRequestIds[1] = {0};
#define WALLET_COUNT 0
#endif
int balancesReceived = 0;
double totalXrp = 0;
unsigned long lastPortfolioRequest = 0;
bool portfolioLoaded = false;

// Button state
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
#define DEBOUNCE_DELAY 50

// Get oldest price for comparison
double getOldestPrice() {
    if (priceHistoryCount == 0) return 0;
    unsigned long oldestTime = ULONG_MAX;
    double oldestPrice = 0;
    for (int i = 0; i < priceHistoryCount; i++) {
        if (priceTimestamps[i] < oldestTime) {
            oldestTime = priceTimestamps[i];
            oldestPrice = priceHistory[i];
        }
    }
    return oldestPrice;
}

void recordPrice(double price) {
    if (price <= 0 || price >= 10000) return;
    priceHistory[priceHistoryIndex] = price;
    priceTimestamps[priceHistoryIndex] = millis();
    priceHistoryIndex = (priceHistoryIndex + 1) % PRICE_HISTORY_SIZE;
    if (priceHistoryCount < PRICE_HISTORY_SIZE) priceHistoryCount++;
}

void checkPriceAlert(double currentPrice) {
    if (currentPrice <= 0) return;
    double oldPrice = getOldestPrice();
    if (oldPrice <= 0) {
        percentChange = 0;
        return;
    }
    percentChange = ((currentPrice - oldPrice) / oldPrice) * 100.0;
    if (!alertActive && fabs(percentChange) >= ALERT_THRESHOLD_PERCENT) {
        alertActive = true;
        alertStartTime = millis();
    }
    if (alertActive && (millis() - alertStartTime >= ALERT_FLASH_DURATION_MS)) {
        alertActive = false;
    }
}

void initDisplay() {
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(100);
    digitalWrite(OLED_RST, HIGH);
    delay(100);
    Wire.begin(OLED_SDA, OLED_SCL);
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
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        showStatus("WiFi: OK", WiFi.localIP().toString().c_str());
    } else {
        showStatus("WiFi: FAILED");
    }
    delay(1000);
}

void connectWebSocket() {
    String wsUrl = "wss://" + String(XRPL_WS_HOST) + ":" + String(XRPL_WS_PORT) + XRPL_WS_PATH;
    showStatus("Connecting XRPL...", XRPL_WS_HOST);
    wsClient.setCACert(letsencrypt_root_ca);
    wsConnected = wsClient.connect(wsUrl);
}

void requestPrice() {
    if (!wsConnected) return;
    
    // Bid side
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
    
    // Ask side
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

void requestPortfolio() {
#if WALLET_COUNT > 0
    if (!wsConnected) return;
    balancesReceived = 0;
    totalXrp = 0;
    
    for (int i = 0; i < WALLET_COUNT; i++) {
        walletBalances[i] = 0;
        JsonDocument req;
        balanceRequestIds[i] = requestId;
        req["id"] = requestId++;
        req["command"] = "account_info";
        req["account"] = WALLET_ADDRESSES[i];
        req["ledger_index"] = "validated";
        String json;
        serializeJson(req, json);
        wsClient.send(json);
    }
#endif
}

void onMessage(WebsocketsMessage msg) {
    JsonDocument doc;
    if (deserializeJson(doc, msg.data())) return;
    
    int msgId = doc["id"] | 0;
    const char* status = doc["status"];
    
    // Check portfolio responses
#if WALLET_COUNT > 0
    for (int i = 0; i < WALLET_COUNT; i++) {
        if (msgId == balanceRequestIds[i]) {
            if (status && strcmp(status, "success") == 0) {
                const char* balanceStr = doc["result"]["account_data"]["Balance"];
                if (balanceStr) {
                    double drops = atof(balanceStr);
                    walletBalances[i] = drops / 1000000.0;
                    totalXrp += walletBalances[i];
                }
            }
            balancesReceived++;
            if (balancesReceived >= WALLET_COUNT) {
                portfolioLoaded = true;
            }
            return;
        }
    }
#endif
    
    // Price responses
    if (!status || strcmp(status, "success") != 0) return;
    
    JsonArray offers = doc["result"]["offers"].as<JsonArray>();
    if (offers.size() == 0) return;
    
    JsonObject offer = offers[0];
    JsonVariant tg = offer["TakerGets"];
    
    if (tg.is<JsonObject>()) {
        const char* quoteStr = tg["value"].as<const char*>();
        const char* dropsStr = offer["TakerPays"].as<const char*>();
        double quote = quoteStr ? atof(quoteStr) : 0;
        double drops = dropsStr ? atof(dropsStr) : 0;
        double xrp = drops / 1000000.0;
        if (quote > 0 && xrp > 0) {
            bestAsk = quote / xrp;
        }
    } else {
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
    } else if (event == WebsocketsEvent::ConnectionClosed) {
        wsConnected = false;
    }
}

void displayTicker() {
    display->clear();
    
    if (alertActive) {
        display->invertDisplay();
    } else {
        display->normalDisplay();
    }
    
    if (xrpPrice > 0 && xrpPrice < 10000) {
        char price[24];
        char format[16];
        snprintf(format, sizeof(format), "%s%%.%df", QUOTE_SYMBOL, QUOTE_DECIMALS);
        snprintf(price, sizeof(price), format, xrpPrice);
        
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 0, DISPLAY_LABEL);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 0, wsConnected ? "LIVE" : "...");
        
        display->setFont(ArialMT_Plain_24);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 18, price);
        
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        if (priceHistoryCount >= 2) {
            char changeStr[20];
            if (fabs(percentChange) >= 0.001) {
                const char* arrow = percentChange >= 0 ? "^ " : "v ";
                snprintf(changeStr, sizeof(changeStr), "%s%.3f%%", arrow, fabs(percentChange));
            } else {
                snprintf(changeStr, sizeof(changeStr), "-- 0.00%%");
            }
            display->drawString(64, 44, changeStr);
        }
        
        char bid[16], ask[16];
        snprintf(format, sizeof(format), "%%.%df", QUOTE_DECIMALS);
        snprintf(bid, sizeof(bid), format, bestBid);
        snprintf(ask, sizeof(ask), format, bestAsk);
        display->setTextAlignment(TEXT_ALIGN_LEFT);
        display->drawString(0, 54, bid);
        display->setTextAlignment(TEXT_ALIGN_RIGHT);
        display->drawString(128, 54, ask);
    } else {
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 8, DISPLAY_LABEL);
        display->setFont(ArialMT_Plain_10);
        display->drawString(64, 32, wsConnected ? "Fetching price..." : "Connecting...");
    }
    
    display->display();
}

void displayPortfolio() {
    display->clear();
    display->normalDisplay();
    
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_LEFT);
    display->drawString(0, 0, "XRP Portfolio");
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(128, 0, wsConnected ? "LIVE" : "...");
    
#if WALLET_COUNT > 0
    if (totalXrp > 0) {
        char xrpStr[24];
        snprintf(xrpStr, sizeof(xrpStr), "%.2f XRP", totalXrp);
        display->setFont(ArialMT_Plain_16);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 14, xrpStr);
        
        if (xrpPrice > 0) {
            double totalUsd = totalXrp * xrpPrice;
            char usdStr[24];
            if (totalUsd >= 1000) {
                snprintf(usdStr, sizeof(usdStr), "%s%.2fK", QUOTE_SYMBOL, totalUsd / 1000.0);
            } else {
                snprintf(usdStr, sizeof(usdStr), "%s%.2f", QUOTE_SYMBOL, totalUsd);
            }
            display->setFont(ArialMT_Plain_24);
            display->drawString(64, 32, usdStr);
        }
        
        display->setFont(ArialMT_Plain_10);
        char priceStr[16];
        snprintf(priceStr, sizeof(priceStr), "@ $%.4f", xrpPrice);
        display->drawString(64, 54, priceStr);
    } else {
        display->setFont(ArialMT_Plain_10);
        display->setTextAlignment(TEXT_ALIGN_CENTER);
        display->drawString(64, 28, portfolioLoaded ? "No balance" : "Loading...");
    }
#else
    display->setFont(ArialMT_Plain_10);
    display->setTextAlignment(TEXT_ALIGN_CENTER);
    display->drawString(64, 28, "No wallets configured");
#endif
    
    display->display();
}

void checkButton() {
    static bool buttonPressed = false;
    bool reading = digitalRead(BUTTON_PIN);
    
    // Simple press detection with debounce
    if (reading == LOW && !buttonPressed) {
        delay(50);  // Simple debounce
        if (digitalRead(BUTTON_PIN) == LOW) {
            buttonPressed = true;
            
            // Button pressed - cycle mode
            displayMode = (displayMode + 1) % 2;
            Serial.printf("Button pressed! Mode: %d\n", displayMode);
            
            // Request portfolio data when switching to portfolio mode
            if (displayMode == MODE_PORTFOLIO && !portfolioLoaded) {
                requestPortfolio();
            }
        }
    }
    
    if (reading == HIGH) {
        buttonPressed = false;
    }
}

void setup() {
    // Reset OLED first
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(200);
    digitalWrite(OLED_RST, HIGH);
    delay(200);
    
    // Button setup
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== XRP Ticker + Portfolio ===");
    
    initDisplay();
    showStatus("XRP Ticker", "Initializing...");
    delay(1000);
    
    connectWiFi();
    
    wsClient.onMessage(onMessage);
    wsClient.onEvent(onEvent);
    connectWebSocket();
}

void loop() {
    static unsigned long lastPriceRecord = 0;
    
    // Check button
    checkButton();
    
    if (wsConnected) {
        wsClient.poll();
        
        // Request price regularly
        if (millis() - lastPriceRequest >= PRICE_UPDATE_INTERVAL) {
            requestPrice();
            lastPriceRequest = millis();
        }
        
        // Record price for alerts
        if (xrpPrice > 0 && millis() - lastPriceRecord >= 5000) {
            recordPrice(xrpPrice);
            checkPriceAlert(xrpPrice);
            lastPriceRecord = millis();
        }
        
        // Refresh portfolio periodically when in that mode
#if WALLET_COUNT > 0
        if (displayMode == MODE_PORTFOLIO && millis() - lastPortfolioRequest >= 30000) {
            requestPortfolio();
            lastPortfolioRequest = millis();
        }
#endif
    } else {
        connectWebSocket();
        delay(5000);
    }
    
    // Check alert timeout
    if (alertActive && (millis() - alertStartTime >= ALERT_FLASH_DURATION_MS)) {
        alertActive = false;
    }
    
    // Update display based on mode
    if (displayMode == MODE_TICKER) {
        displayTicker();
    } else {
        displayPortfolio();
    }
    
    delay(50);
}
