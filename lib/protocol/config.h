#pragma once
// config for both esps
#include <stdint.h>

// wifi channel used by esp-now, when the wifi ap connects to the host phone 
// ap canal must be the same
#define ESPNOW_CHANNEL 1

// mac adresses
static const uint8_t HOST_MAC[6]   = {0x7C, 0x9E, 0xBD, 0x39, 0x9E, 0x5C};
static const uint8_t CLIENT_MAC[6] = {0xAC, 0x67, 0xB2, 0x2E, 0xCF, 0x4C};

// buttons
#define PIN_BTN_LEFT  25
#define PIN_BTN_RIGHT 26

#define TICK_MS 16   // ~60 Hz