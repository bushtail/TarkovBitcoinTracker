#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ctime>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

#include "Config.h"
#include "secrets.h"
#include "PriceDisplay.h"
#include "TarkovDevApiClient.h"

/**
 * Tarkov item price tracker
 * Used to be just for bitcoin early on in development
 * By: bushtail
 */
namespace
{
    const GameMode gameMode(PVE);

    PriceDisplay display(
        LCD_I2C_ADDRESS,
        LCD_COLS,
        LCD_ROWS,
        gameMode,
        USE_FLEA_PRICE);

    TarkovDevApiClient api(gameMode, TRACKED_ITEM_ID);

    unsigned long lastWifiAttempt = 0;
    unsigned long lastFetch = 0;
    unsigned long fetchInterval = RETRY_INTERVAL_MS;

    bool wasConnected = false;
    bool fetchImmediately = true;
    bool clockWarningShown = false;

    unsigned long clockStarted = 0;
    QueueHandle_t priceResults = nullptr;
    TaskHandle_t fetchTask = nullptr;
    PriceData fetchedPrice;
    bool fetchInProgress = false;

    void fetchWorker(void*)
    {
        for (;;)
        {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            fetchedPrice = api.fetchItemPrice();
            constexpr bool done = true;
            xQueueSend(priceResults, &done, portMAX_DELAY);
        }
    }
}

void setup()
{
    Serial.begin(115200);

    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setTimeOut(50);

    Serial.printf(
        "\nTarkov %s Item Price Tracker\n",
        gameMode.label());

    if (!display.begin())
    {
        Serial.printf(
            "LCD not found at 0x%02X (SDA %u, SCL %u). I2C scan:\n",
            LCD_I2C_ADDRESS,
            I2C_SDA_PIN,
            I2C_SCL_PIN);

        for (uint8_t address = 1; address < 127; ++address)
        {
            Wire.beginTransmission(address);

            if (Wire.endTransmission() == 0)
            {
                Serial.printf(
                    "  Found device at 0x%02X\n",
                    address);
            }
        }
    }

    display.showTrackerStatus("CONNECTING WIFI");

    priceResults = xQueueCreate(1, sizeof(bool));
    if (!priceResults || xTaskCreate(fetchWorker, "price-fetch", 12288, nullptr, 1, &fetchTask) != pdPASS)
    {
        display.showError("INIT FAILED");
        return;
    }

    WiFi.persistent(false);
    WiFiClass::mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(SSID, PASS);

    lastWifiAttempt = millis();
}

void loop()
{
    const unsigned long now = millis();
    display.tick(now);
    if (!fetchTask)
    {
        delay(20);
        return;
    }

    bool resultReady = false;
    if (fetchInProgress && xQueueReceive(priceResults, &resultReady, 0) == pdTRUE)
    {
        fetchInProgress = false;
        lastFetch = now;
        fetchInterval = fetchedPrice.success ? REFRESH_INTERVAL_MS : RETRY_INTERVAL_MS;
        display.showPrice(fetchedPrice);
        if (fetchedPrice.success)
        {
            Serial.printf("%s %s: trader %ld RUB; flea low %ld; 24h average %ld; base %ld\n",
                          gameMode.label(), fetchedPrice.itemName.c_str(), fetchedPrice.traderPrice,
                          fetchedPrice.lastLowPrice, fetchedPrice.avgPrice, fetchedPrice.basePrice);
        }
        else
        {
            Serial.println("Price unavailable; retrying in 30 seconds");
        }
    }

    if (WiFiClass::status() != WL_CONNECTED)
    {
        if (wasConnected)
        {
            Serial.println("Wi-Fi disconnected");
            display.showError("WIFI LOST");

            wasConnected = false;
        }

        if (now - lastWifiAttempt >= WIFI_RETRY_INTERVAL_MS)
        {
            lastWifiAttempt = now;

            display.showTrackerStatus("WIFI RETRY...");
            Serial.println("Retrying Wi-Fi connection");

            WiFi.reconnect();
        }

        delay(20);
        return;
    }

    if (!wasConnected)
    {
        wasConnected = true;
        fetchImmediately = true;
        clockWarningShown = false;
        clockStarted = now;

        Serial.println("Wi-Fi connected");
        display.showTrackerStatus("SYNCING CLOCK");

        configTime(
            0,
            0,
            "pool.ntp.org",
            "time.google.com");
    }

    // TLS certificate validation requires a clock set by NTP.
    if (time(nullptr) < 1704067200)
    {
        if (!clockWarningShown &&
            now - clockStarted >= RETRY_INTERVAL_MS)
        {

            clockWarningShown = true;

            Serial.println(
                "Waiting for NTP; check internet access / UDP port 123");

            display.showError("WAITING FOR TIME");
        }

        delay(20);
        return;
    }

    if (!fetchInProgress && (fetchImmediately || now - lastFetch >= fetchInterval))
    {
        if (fetchImmediately)
        {
            display.showTrackerStatus("FETCHING PRICE");
        }
        fetchImmediately = false;
        fetchInProgress = true;
        xTaskNotifyGive(fetchTask);
    }

    delay(20);
}