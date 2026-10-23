#pragma once
// file for communication with the phone (wifi websocket or ble notify) - TBD
// Mode selection: #define PHONE_MODE_WIFI / PHONE_MODE_BLE or separate environments in platformio.ini
#include "protocol.h"

inline void phone_begin() {}
inline void phone_send_state(const StatePacket&) {}