#pragma once

// Snapshot the user setting once so requests and labels always use the same mode.
class GameMode
{
public:
    explicit constexpr GameMode(const bool pve) : pve_(pve)
    {
    }

    const char* label() const
    {
        return pve_ ? "PvE" : "PvP";
    }

    const char* itemsUrl() const
    {
        return pve_ ? "https://json.tarkov.dev/pve/items"
                    : "https://json.tarkov.dev/regular/items";
    }

    const char* namesUrl() const
    {
        return pve_ ? "https://json.tarkov.dev/pve/items_en"
                    : "https://json.tarkov.dev/regular/items_en";
    }

private:
    bool pve_;
};
