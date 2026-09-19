#include "TarkovDevApiClient.h"

#include "Config.h"
#include "TarkovRootCA.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <memory>

namespace
{
    class BufferedReader
    {
    public:
        explicit BufferedReader(WiFiClient& stream) : stream_(stream)
        {
        }

        // ReSharper disable once CppDeclaratorNeverUsed
        int read()
        {
            if (position_ >= length_)
            {
                if (!fillBuffer())
                {
                    return -1;
                }
            }

            return static_cast<unsigned char>(
                buffer_[position_++]);
        }

        size_t bytesRead = 0;

    private:
        static constexpr size_t BUFFER_SIZE = 1024;

        bool fillBuffer()
        {
            position_ = 0;
            length_ = 0;

            const unsigned long started = millis();

            while (millis() - started < HTTP_TIMEOUT_MS)
            {
                const int available = stream_.available();

                if (available > 0)
                {
                    const size_t requested =
                        static_cast<size_t>(available) < BUFFER_SIZE
                            ? static_cast<size_t>(available)
                            : BUFFER_SIZE;

                    const int received = stream_.read(
                        reinterpret_cast<uint8_t*>(buffer_.get()),
                        requested);

                    if (received > 0)
                    {
                        length_ =
                            static_cast<size_t>(received);

                        bytesRead += length_;

                        reportProgress();

                        return true;
                    }
                }

                if (!stream_.connected())
                {
                    return false;
                }

                delay(1);
            }

            return false;
        }

        void reportProgress()
        {
            const size_t megabytes =
                bytesRead / (1024UL * 1024UL);

            if (megabytes == reportedMegabytes_)
            {
                return;
            }

            reportedMegabytes_ = megabytes;

            Serial.printf(
                "JSON download: %u MiB\n",
                static_cast<unsigned>(reportedMegabytes_));
        }

        WiFiClient& stream_;

        std::unique_ptr<char[]> buffer_{
            new char[BUFFER_SIZE]
        };

        size_t position_ = 0;
        size_t length_ = 0;
        size_t reportedMegabytes_ = 0;
    };

    bool beginGet(
        HTTPClient& http,
        WiFiClientSecure& client,
        const char* url)
    {
        client.setCACert(TARKOV_ROOT_CA);
        client.setHandshakeTimeout(15);

        http.setConnectTimeout(HTTP_TIMEOUT_MS);
        http.setTimeout(HTTP_TIMEOUT_MS);

        http.useHTTP10(true);

        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

        if (!http.begin(client, url))
        {
            Serial.printf(
                "Failed to initialize HTTP request: %s\n",
                url);

            return false;
        }

        const char* headers[] = {
            "Transfer-Encoding",
            "Content-Encoding"
        };

        http.collectHeaders(
            headers,
            sizeof(headers) / sizeof(headers[0]));

        http.addHeader(
            "Accept",
            "application/json");

        http.addHeader(
            "Accept-Encoding",
            "identity");

        const int status = http.GET();

        if (status != HTTP_CODE_OK)
        {
            Serial.printf("Tarkov JSON request failed: HTTP %d (%s)\n", status, url);

            return false;
        }

        const String transferEncoding = http.header("Transfer-Encoding");

        if (transferEncoding.length() > 0 && !transferEncoding.equalsIgnoreCase("identity"))
        {
            Serial.printf(
                "Unsupported Transfer-Encoding: %s\n",
                transferEncoding.c_str());

            return false;
        }

        const String contentEncoding = http.header("Content-Encoding");

        if (contentEncoding.length() > 0 && !contentEncoding.equalsIgnoreCase("identity"))
        {
            Serial.printf(
                "Unsupported Content-Encoding: %s\n",
                contentEncoding.c_str());

            return false;
        }

        return true;
    }
}

PriceData TarkovDevApiClient::fetchItemPrice() const
{
    WiFiClientSecure client;
    HTTPClient http;

    if (!beginGet(http, client, mode_.itemsUrl()))
    {
        http.end();
        return {};
    }

    BufferedReader items{
        http.getStream()
    };

    PriceData result =
        parseItems(items, itemId_.c_str());

    Serial.printf(
        "Items JSON: read %u bytes (HTTP length %d)\n",
        static_cast<unsigned>(items.bytesRead),
        http.getSize());

    client.stop();
    http.end();

    if (!result.success)
    {
        Serial.println(
            "Tarkov JSON feed invalid or item price missing");
    }

    if (result.success && result.itemNameKey.length())
    {
        if (beginGet(http, client, mode_.namesUrl()))
        {
            BufferedReader names{
                http.getStream()
            };
            const String name = parseName(names, result.itemNameKey);
            if (name.length())
            {
                result.itemName = name;
            }
        }
        client.stop();
        http.end();
    }

    return result;
}
