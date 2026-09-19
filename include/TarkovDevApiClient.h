#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "GameMode.h"
#include "JsonFeedReader.h"

enum class PriceSource
{
    None,
    Trader,
    FleaLow,
    FleaAverage,
    Base
};

struct PriceQuote
{
    explicit PriceQuote(const long value = -1, const PriceSource kind = PriceSource::None)
        : price(value), source(kind)
    {
    }
    long price;
    PriceSource source;
};

struct PriceData
{
    bool success = false;
    String itemId = "";
    String itemName = "";
    String itemNameKey = "";
    long avgPrice = -1;
    long lastLowPrice = -1;
    long traderPrice = -1;
    long basePrice = -1;
    bool fleaAllowed = true;
    PriceQuote selectPrice(bool preferFlea = false) const;
};

class TarkovDevApiClient
{
public:
    TarkovDevApiClient(const GameMode mode, const char* itemId)
        : mode_(mode), itemId_(itemId ? itemId : "")
    {
    }

    PriceData fetchItemPrice() const;

    static PriceData parseResponse(const String& json, const char* itemId);

    template <class Reader>
    static PriceData parseItems(Reader& input, const char* itemId)
    {
        if (!itemId || !itemId[0])
        {
            return {};
        }

        JsonFeedReader<Reader> cursor(input);

        if (!cursor.enterMember("data", true) ||
            !cursor.enterMember("items") ||
            !cursor.enterMember(itemId))
        {
            return PriceData{};
        }

        JsonDocument filter;
        const JsonObject item = filter.to<JsonObject>();

        item["id"] = true;
        item["name"] = true;
        item["normalizedName"] = true;
        item["avg24hPrice"] = true;
        item["basePrice"] = true;
        item["lastLowPrice"] = true;
        item["types"] = true;

        const JsonObject offer = item["sellToTrader"].to<JsonArray>().add<JsonObject>();

        offer["trader"] = true;
        offer["currency"] = true;
        offer["price"] = true;
        offer["priceRUB"] = true;

        JsonDocument doc;

        const DeserializationError error =
            deserializeJson(
                doc,
                cursor,
                DeserializationOption::Filter(filter));

        if (error)
        {
#ifdef ARDUINO
            Serial.printf("Items JSON parse error: %s\n", error.c_str());
#endif

            return PriceData{};
        }

        return parseItemDocument(doc, itemId);
    }

    template <class Reader>
    static String parseName(Reader& input, const String& key)
    {
        JsonFeedReader<Reader> cursor(input);
        if (!cursor.enterMember("data") || !cursor.enterMember(key.c_str()))
        {
            return "";
        }
        JsonDocument name;
        if (deserializeJson(name, cursor) || !name.is<const char*>())
        {
            return "";
        }
        return name.as<const char*>();
    }

private:
    GameMode mode_;
    String itemId_;

    static PriceData parseItemDocument(const JsonDocument& doc, const char* itemId);
};