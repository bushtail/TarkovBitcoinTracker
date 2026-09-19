#pragma once

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include "TarkovDevApiClient.h"

class PriceDisplay
{
public:
    PriceDisplay(uint8_t address, uint8_t columns, uint8_t rows, GameMode mode, bool preferFlea = false);

    bool begin();
    void tick(uint32_t now);

    void showPrice(const PriceData& data);
    void showTrackerStatus(const String& message);
    void showError(const String& message);

private:
    static String formatPriceWithCommas(long price);
    void writeHeading();
    String itemName_ = "Item";
    static int currencyGlyph(const String& itemId);
    void writePriceLine(long price, int sourceGlyph, int direction);
    void writeInitTitle();
    void writeLine(uint8_t row, const String& text);
    void writeCenteredLine(uint8_t row, const String& text);

    LiquidCrystal_I2C _lcd;
    GameMode mode_;
    bool preferFlea_;
    uint8_t _address;
    uint8_t _columns;
    uint8_t _rows;
    bool _ready = false;
    bool initializing_ = true;
    bool initTitleVisible_ = false;
    uint32_t lastScroll_ = 0;
    unsigned int scrollOffset_ = 0;
    long previousPrice_ = -1;
    String previousItemId_;
    PriceSource previousSource_ = PriceSource::None;
};
