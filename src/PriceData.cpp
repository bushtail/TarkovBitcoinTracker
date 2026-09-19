#include "TarkovDevApiClient.h"

#include <cstring>
#include <cctype>

PriceQuote PriceData::selectPrice(const bool preferFlea) const
{
    PriceQuote flea;
    if (fleaAllowed)
    {
        if (lastLowPrice > 0)
        {
            flea = PriceQuote{
                lastLowPrice,
                PriceSource::FleaLow
            };
        }
        else if (avgPrice > 0)
        {
            flea = PriceQuote{
                avgPrice,
                PriceSource::FleaAverage
            };
        }
    }

    if (preferFlea && flea.price > 0)
    {
        return flea;
    }

    if (traderPrice > 0)
    {
        return PriceQuote{
            traderPrice,
            PriceSource::Trader
        };
    }
    if (flea.price > 0)
    {
        return flea;
    }
    if (basePrice > 0)
    {
        return PriceQuote{
            basePrice,
            PriceSource::Base
        };
    }
    return PriceQuote{};
}

PriceData TarkovDevApiClient::parseResponse(const String& json, const char* itemId)
{
    struct StringReader
    {
        const char* text;

        int read()
        {
            return *text ? static_cast<unsigned char>(*text++) : -1;
        }
    } input{
        json.c_str()
    };

    return parseItems(input, itemId);
}

PriceData TarkovDevApiClient::parseItemDocument(const JsonDocument& doc, const char* itemId)
{
    PriceData result;

    if (!doc["errors"].isNull() && (!doc["errors"].is<JsonArrayConst>() || doc["errors"].size() != 0))
    {
        return result;
    }

    const JsonObjectConst item = doc.as<JsonObjectConst>();

    if (item.isNull() ||
        std::strcmp(item["id"] | "", itemId) != 0)
    {
        return result;
    }

    result.itemId = item["id"].as<const char*>();
    const char* name = item["name"] | "";
    const String key = String(itemId) + " Name";
    if (std::strcmp(name, key.c_str()) == 0)
    {
        result.itemNameKey = name;
    }
    else if (name[0] && std::strcmp(name, itemId) != 0)
    {
        result.itemName = name;
    }

    if (!result.itemName.length())
    {
        const char* normalized = item["normalizedName"] | "";
        bool capitalize = true;
        for (const char* c = normalized; *c; ++c)
        {
            if (*c == '-' || *c == '_')
            {
                result.itemName += ' ';
                capitalize = true;
            }
            else
            {
                result.itemName += capitalize ? static_cast<char>(std::toupper(static_cast<unsigned char>(*c))) : *c;
                capitalize = false;
            }
        }
        if (!result.itemName.length())
        {
            result.itemName = "Item";
        }
    }

    for (JsonVariantConst type : item["types"].as<JsonArrayConst>())
    {
        if (std::strcmp(type | "", "noFlea") == 0)
        {
            result.fleaAllowed = false;
        }
    }

    result.basePrice = item["basePrice"] | -1L;

    result.avgPrice = item["avg24hPrice"] | -1L;

    result.lastLowPrice = item["lastLowPrice"] | -1L;

    for (const JsonObjectConst offer : item["sellToTrader"].as<JsonArrayConst>())
    {
        const char* currency = offer["currency"] | "";
        const char* trader = offer["trader"] | "";
        const long price = std::strcmp(currency, "RUB") == 0 ? offer["price"] | -1L : offer["priceRUB"] | -1L;

        if (trader[0] != '\0' && price > result.traderPrice)
        {
            result.traderPrice = price;
        }
    }

    result.success = result.selectPrice().price > 0;

    return result;
}
