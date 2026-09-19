#include "PriceDisplay.h"
#include <cstring>

PriceDisplay::PriceDisplay(const uint8_t address, const uint8_t columns, const uint8_t rows, const GameMode mode, const bool preferFlea)
    : _lcd(address, columns, rows), mode_(mode), preferFlea_(preferFlea), _address(address), _columns(columns), _rows(rows)
{
}

bool PriceDisplay::begin()
{
    Wire.beginTransmission(_address);
    _ready = Wire.endTransmission() == 0;

    if (!_ready)
    {
        return false;
    }

    _lcd.init();
    _lcd.backlight();

    uint8_t bitcoin[8] = {
        0b01010,
        0b11110,
        0b01001,
        0b01110,
        0b01001,
        0b11110,
        0b01010,
        0000000
    };

    uint8_t ruble[8] = {
        0b11110,
        0b10001,
        0b10001,
        0b11110,
        0b10000,
        0b11100,
        0b10000,
        0000000
    };

    uint8_t gp[8] = {
        0b01110,
        0b10001,
        0b10111,
        0b10101,
        0b10111,
        0b10001,
        0b01110,
        0000000
    };

    uint8_t up[8] = {
        0b00100,
        0b01110,
        0b10101,
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0000000
    };

    uint8_t down[8] = {
        0b00100,
        0b00100,
        0b00100,
        0b00100,
        0b10101,
        0b01110,
        0b00100,
        0000000
    };

    _lcd.createChar(3, up);
    _lcd.createChar(4, down);
    _lcd.createChar(2, gp);
    _lcd.createChar(0, bitcoin);
    _lcd.createChar(1, ruble);

    return true;
}

void PriceDisplay::showPrice(const PriceData& data)
{
    if (!data.success)
    {
        showError("FETCH FAILED");
        return;
    }

    itemName_ = data.itemName.length() ? data.itemName : "Item";

    if (std::strcmp(itemName_.c_str(), "Physical Bitcoin") == 0 || std::strcmp(itemName_.c_str(), "Physical bitcoin") == 0)
    {
        itemName_ = "Bitcoin";
    }

    const PriceQuote quote = data.selectPrice(preferFlea_);
    const long price = quote.price;

    if (price <= 0)
    {
        showError("NO PRICE DATA");
        return;
    }

    const PriceSource source = quote.source;
    int direction = 0;

    if (previousPrice_ > 0 && data.itemId.length() && std::strcmp(previousItemId_.c_str(), data.itemId.c_str()) == 0 && previousSource_ == source)
    {
        direction = price > previousPrice_ ? 1 : price < previousPrice_ ? -1
                                                                        : 0;
    }
    previousPrice_ = price;
    previousItemId_ = data.itemId;
    previousSource_ = source;
    initializing_ = false;
    writeHeading();
    writePriceLine(price, currencyGlyph(data.itemId), direction);
}

int PriceDisplay::currencyGlyph(const String& itemId)
{
    const char* id = itemId.c_str();
    if (std::strcmp(id, "5449016a4bdc2d6f028b456f") == 0)
    {
        return 1; // RUB
    }
    if (std::strcmp(id, "5696686a4bdc2da3298b456a") == 0)
    {
        return '$'; // USD
    }
    if (std::strcmp(id, "59faff1d86f7746c51718c9c") == 0)
    {
        return 0; // BTC
    }
    if (std::strcmp(id, "5d235b4d86f7742e017bc88a") == 0)
    {
        return 2; // GP
    }
    return -1;
}

void PriceDisplay::writePriceLine(const long price, const int sourceGlyph, const int direction)
{
    if (!_ready || _rows < 2 || _columns < 2)
    {
        return;
    }

    String amount = formatPriceWithCommas(price);

    const bool conversion = sourceGlyph >= 0;

    const unsigned int arrowWidth = direction == 0 ? 0 : 1;
    const unsigned int reserved = (conversion ? 4 : 1) + arrowWidth;

    if (amount.length() + reserved > _columns)
    {
        amount = String(price);
    }

    if (amount.length() + reserved > _columns)
    {
        writeLine(1, "PRICE TOO LONG");
        return;
    }

    const uint8_t rightStart = _columns - amount.length() - 1 - arrowWidth;

    writeLine(1, "");

    if (conversion)
    {
        const uint8_t equalsColumn = (2 + rightStart - 1) / 2;
        _lcd.setCursor(0, 1);
        _lcd.write(static_cast<uint8_t>(sourceGlyph));
        _lcd.write('1');
        _lcd.setCursor(equalsColumn, 1);
        _lcd.write('=');
    }

    _lcd.setCursor(rightStart, 1);
    _lcd.write(1);

    for (unsigned int i = 0; i < amount.length(); ++i)
    {
        _lcd.write(amount[i]);
    }

    if (direction != 0)
    {
        _lcd.setCursor(_columns - 1, 1);
        _lcd.write(static_cast<uint8_t>(direction > 0 ? 3 : 4));
    }
}

void PriceDisplay::writeInitTitle()
{
    if (!_ready || !_rows)
    {
        return;
    }
    static constexpr char title[] = "TARKOV PRICE TRACKER   ";
    constexpr unsigned int length = sizeof(title) - 1;
    _lcd.setCursor(0, 0);

    for (unsigned int column = 0; column < _columns; ++column)
    {
        _lcd.write(title[(scrollOffset_ + column) % length]);
    }
}

void PriceDisplay::tick(const uint32_t now)
{
    if (!initializing_ || !initTitleVisible_ || !_ready)
    {
        return;
    }

    const uint32_t steps = (now - lastScroll_) / 400;

    if (!steps)
    {
        return;
    }

    lastScroll_ += steps * 400;
    scrollOffset_ = (scrollOffset_ + steps) % (sizeof("TARKOV PRICE TRACKER   ") - 1);
    writeInitTitle();
}

void PriceDisplay::writeHeading()
{
    if (!_ready || !_rows || _columns < 4)
    {
        return;
    }

    const uint8_t modeStart = _columns - 3;

    _lcd.setCursor(0, 0);

    for (uint8_t column = 0; column < modeStart; ++column)
    {
        _lcd.write(column < modeStart - 1 && column < itemName_.length() ? itemName_[column] : ' ');
    }

    for (uint8_t i = 0; i < 3; ++i)
    {
        _lcd.write(mode_.label()[i]);
    }
}

void PriceDisplay::showTrackerStatus(const String& message)
{
    if (initializing_)
    {
        if (!initTitleVisible_)
        {
            initTitleVisible_ = true;
            scrollOffset_ = 0;
        }

        writeInitTitle();
        writeCenteredLine(1, message);
        return;
    }
    writeHeading();
    writeLine(1, message);
}

void PriceDisplay::showError(const String& message)
{
    showTrackerStatus(message);
}

void PriceDisplay::writeCenteredLine(const uint8_t row, const String& text)
{
    if (!_ready || row >= _rows)
    {
        return;
    }

    const unsigned int length = text.length() < _columns ? text.length() : _columns;
    const unsigned int padding = (_columns - length) / 2;
    _lcd.setCursor(0, row);

    for (unsigned int column = 0; column < _columns; ++column)
    {
        _lcd.write(column >= padding && column < padding + length ? text[column - padding] : ' ');
    }
}

void PriceDisplay::writeLine(const uint8_t row, const String& text)
{
    if (!_ready || row >= _rows)
    {
        return;
    }

    _lcd.setCursor(0, row);

    for (uint8_t column = 0; column < _columns; ++column)
    {
        _lcd.write(column < text.length() ? text[column] : ' ');
    }
}

String PriceDisplay::formatPriceWithCommas(const long price)
{
    if (price < 0)
    {
        return "N/A";
    }

    const String digits(price);
    String formatted;

    for (unsigned int i = 0; i < digits.length(); ++i)
    {
        if (i > 0 && (digits.length() - i) % 3 == 0)
        {
            formatted += ',';
        }

        formatted += digits[i];
    }

    return formatted;
}