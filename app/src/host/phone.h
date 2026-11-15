#pragma once
// Połączenie z telefonem: Wi-Fi (AP + WebSocket) ORAZ BLE jednocześnie
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsServer.h>
#include <NimBLEDevice.h>
#include "config.h"
#include "protocol.h"

#define PHONE_SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define PHONE_CHAR_UUID    "6e400002-b5a3-f393-e0a9-e50e24dcca9e"

// ---------- Wi-Fi: AP + WebSocket (ws://192.168.4.1:81) ----------
static WebSocketsServer wsServer(81);

// ---------- BLE: jedna charakterystyka NOTIFY ----------
static NimBLECharacteristic* phoneChar = nullptr;
static volatile bool bleConnected = false;

class PhoneSrvCb : public NimBLEServerCallbacks {
  // NimBLE-Arduino 1.x (bez override, żeby nie zależeć od drobnych różnic sygnatur)
  void onConnect(NimBLEServer*) {
    bleConnected = true;
    Serial.println("BLE: connected");
  }
  void onDisconnect(NimBLEServer*) {
    bleConnected = false;
    Serial.println("BLE: disconnected");
    NimBLEDevice::startAdvertising();
  }
};

// Wywołać PO WiFi.mode(WIFI_AP_STA)
inline void phone_begin() {
  // Wi-Fi AP na tym samym kanale co ESP-NOW
  bool ok = WiFi.softAP("PingPong", "pingpong123", ESPNOW_CHANNEL);
  Serial.printf("softAP: %s, IP %s\n", ok ? "OK" : "FAILED",
                WiFi.softAPIP().toString().c_str());
  wsServer.begin();

  // BLE
  NimBLEDevice::init("PingPong");
  NimBLEServer* srv = NimBLEDevice::createServer();
  srv->setCallbacks(new PhoneSrvCb());
  NimBLEService* svc = srv->createService(PHONE_SERVICE_UUID);
  phoneChar = svc->createCharacteristic(PHONE_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
  svc->start();
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(PHONE_SERVICE_UUID);
  adv->start();
  Serial.println("BLE: advertising as PingPong");
}

inline void phone_send_state(const StatePacket& p) {
  // Wi-Fi: pełne 60 Hz, tylko gdy ktoś jest połączony
  wsServer.loop();
  if (wsServer.connectedClients() > 0)
    wsServer.broadcastBIN((const uint8_t*)&p, sizeof(p));

  // BLE: co 2. pakiet (~30 Hz)
  static uint8_t n = 0;
  if (bleConnected && !(n++ % 2)) {
    phoneChar->setValue((const uint8_t*)&p, sizeof(p));
    phoneChar->notify();
  }
}