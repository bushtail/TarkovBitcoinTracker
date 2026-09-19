#pragma once

// Copy to secrets.h and enter your 2.4 GHz Wi-Fi credentials.
static auto SSID = "your-wifi-name";
static auto PASS = "your-wifi-password";
static bool PVE = true;

// Any item ID from the selected mode's items feed. Rebuild and upload after changing.
static constexpr auto TRACKED_ITEM_ID = "59faff1d86f7746c51718c9c";

// Prefer flea prices for eligible items; otherwise use the best trader offer.
static constexpr bool USE_FLEA_PRICE = false;
