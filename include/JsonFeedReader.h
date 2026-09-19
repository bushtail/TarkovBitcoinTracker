#pragma once
#include <ArduinoJson.h>
#include <cstring>

// A one-byte lookahead lets us walk object members without loading their values.
// Input follows ArduinoJson's custom-reader interface: read() and readBytes().
template <class Input>
class JsonFeedReader
{
public:
    explicit JsonFeedReader(Input& input) : input_(input)
    {
    }
    int read()
    {
        const int value = peek();
        cached_ = -2;
        return value;
    }
    size_t readBytes(char* buffer, const size_t count)
    {
        size_t n = 0;
        while (n < count)
        {
            const int value = read();
            if (value < 0)
            {
                break;
            }
            buffer[n++] = static_cast<char>(value);
        }
        return n;
    }
    bool enterMember(const char* name, const bool checkErrors = false)
    {
        if (next() != '{')
        {
            return false;
        }
        for (;;)
        {
            skipSpace();
            if (peek() != '"')
            {
                return false;
            }
            JsonDocument key;
            if (deserializeJson(key, *this) || next() != ':')
            {
                return false;
            }
            const auto field = key.as<const char*>();
            if (!field)
            {
                return false;
            }
            if (std::strcmp(field, name) == 0)
            {
                return true;
            }
            if (checkErrors && std::strcmp(field, "errors") == 0)
            {
                JsonDocument errors;
                if (deserializeJson(errors, *this))
                {
                    return false;
                }
                if (!errors.isNull() && (!errors.is<JsonArray>() || errors.size()))
                {
                    return false;
                }
            }
            else if (!skipValue())
            {
                return false;
            }
            if (next() != ',')
            {
                return false;
            }
        }
    }

private:
    int peek()
    {
        if (cached_ == -2)
        {
            cached_ = input_.read();
        }
        return cached_;
    }
    void skipSpace()
    {
        while (peek() == ' ' || peek() == '\r' || peek() == '\n' || peek() == '\t')
        {
            read();
        }
    }
    int next()
    {
        skipSpace();
        return read();
    }
    bool skipValue()
    {
        skipSpace();
        if (peek() == '{' || peek() == '[' || peek() == '"')
        {
            JsonDocument ignored, filter;
            filter.set(false);
            return !deserializeJson(ignored, *this, DeserializationOption::Filter(filter));
        }
        // Scalars need lookahead because ArduinoJson may read their delimiter.
        char scalar[64];
        size_t count = 0;
        while (peek() >= 0 && peek() != ',' && peek() != '}')
        {
            if (count == sizeof(scalar) - 1)
            {
                return false;
            }
            scalar[count++] = static_cast<char>(read());
        }
        scalar[count] = '\0';
        JsonDocument value;
        return count && !deserializeJson(value, scalar);
    }
    Input& input_;
    int cached_ = -2;
};
