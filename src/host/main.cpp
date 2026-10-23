// ESP32 #1 - host: game logic and physics, receive paddle 2 position, send state
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "config.h"
#include "protocol.h"
#include "game.h"
#include "phone.h"

static game::Game g;
static uint16_t txSeq = 0;

// data from ESP-NOW callback (wifi thread) -> main loop
static volatile int16_t rxPaddle2 = game::FIELD_W / 2;
static volatile uint32_t rxCount = 0, rxLost = 0;
static volatile uint32_t lastRxMs = 0;
static volatile uint16_t lastSeq = 0;
static volatile bool haveSeq = false;

// Arduino-ESP32 core 2.x. W core 3.x: (const esp_now_recv_info_t* info, const uint8_t* data, int len)
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(InputPacket) || data[0] != PKT_INPUT) return;
  InputPacket p;
  memcpy(&p, data, sizeof(p));
  if (haveSeq) rxLost += (uint16_t)(p.seq - lastSeq - 1);
  lastSeq = p.seq;
  haveSeq = true;
  rxPaddle2 = p.paddle_x;
  rxCount++;
  lastRxMs = millis();
}

static void onSent(const uint8_t* mac, esp_now_send_status_t status) {
// optional: count failed sends (status != ESP_NOW_SEND_SUCCESS)
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("HOST MAC: %s\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init FAILED");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, CLIENT_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("add_peer FAILED");

  game::init(g);
  phone_begin();
}

void loop() {
  static uint32_t lastTick = 0, lastLog = 0;
  uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  float dt = (now - lastTick) / 1000.0f;
  lastTick = now;

  // 1. local input (player 1)
  bool left = digitalRead(PIN_BTN_LEFT) == LOW;
  bool right = digitalRead(PIN_BTN_RIGHT) == LOW;
  float x = g.p1x;
  if (left)  x -= game::PADDLE_SPEED * dt;
  if (right) x += game::PADDLE_SPEED * dt;
  game::setPaddle1(g, x);

  // restart after game over: both buttons at the same time
  if (g.status == ST_GAME_OVER && left && right) game::restart(g);

  // 2. players 2 paddle from ESP-NOW
  game::setPaddle2(g, rxPaddle2);

  // 3. physics
  game::step(g, dt);

  // 4. send state
  StatePacket sp;
  sp.type = PKT_STATE;
  sp.seq = txSeq++;
  sp.host_t_ms = now;
  game::toPacket(g, sp);
  esp_now_send(CLIENT_MAC, (const uint8_t*)&sp, sizeof(sp));
  phone_send_state(sp);

  if (now - lastLog > 1000) {
    lastLog = now;
    Serial.printf("score %u:%u | rx=%lu lost=%lu | last rx %lu ms ago\n", g.score1, g.score2,
                  (unsigned long)rxCount, (unsigned long)rxLost, (unsigned long)(now - lastRxMs));
  }
}