// ESP32 #2 - client: reading buttons, sending paddle position, receiving game state
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>
#include "config.h"
#include "protocol.h"
#include "game.h"

static float paddleX = game::FIELD_W / 2.0f;
static uint16_t txSeq = 0;

static StatePacket latest;
static volatile bool haveState = false;
static volatile uint32_t rxCount = 0, rxLost = 0;
static volatile uint16_t lastSeq = 0;
static volatile bool haveSeq = false;

// Arduino-ESP32 core 2.x. W core 3.x: (const esp_now_recv_info_t* info, const uint8_t* data, int len)
static void onRecv(const uint8_t* mac, const uint8_t* data, int len) {
  if (len != sizeof(StatePacket) || data[0] != PKT_STATE) return;
  StatePacket p;
  memcpy(&p, data, sizeof(p));
  if (haveSeq) rxLost += (uint16_t)(p.seq - lastSeq - 1);
  lastSeq = p.seq;
  haveSeq = true;
  latest = p;
  haveState = true;
  rxCount++;
}

static void onSent(const uint8_t* mac, esp_now_send_status_t status) {}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  Serial.printf("CLIENT MAC: %s\n", WiFi.macAddress().c_str());

  if (esp_now_init() != ESP_OK) {
    Serial.println("esp_now_init FAILED");
    while (true) delay(1000);
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, HOST_MAC, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) != ESP_OK) Serial.println("add_peer FAILED");
}

void loop() {
  static uint32_t lastTick = 0, lastLog = 0;
  uint32_t now = millis();
  if (now - lastTick < TICK_MS) return;
  float dt = (now - lastTick) / 1000.0f;
  lastTick = now;

  if (digitalRead(PIN_BTN_LEFT) == LOW)  paddleX -= game::PADDLE_SPEED * dt;
  if (digitalRead(PIN_BTN_RIGHT) == LOW) paddleX += game::PADDLE_SPEED * dt;
  paddleX = game::clampPaddle(paddleX);
  
  InputPacket ip;
  ip.type = PKT_INPUT;
  ip.seq = txSeq++;
  ip.paddle_x = (int16_t)paddleX;
  ip.t_ms = now;
  esp_now_send(HOST_MAC, (const uint8_t*)&ip, sizeof(ip));

  if (now - lastLog > 1000) {
    lastLog = now;
    /*if (haveState) {
      Serial.printf("ball (%d,%d) score %u:%u status %u | rx=%lu lost=%lu\n", latest.ball_x,
                    latest.ball_y, latest.score1, latest.score2, latest.status,
                    (unsigned long)rxCount, (unsigned long)rxLost);
    } */
  static uint32_t lastPrinted = 0;
  if (haveState && rxCount != lastPrinted) {
    lastPrinted = rxCount;
    StatePacket s = latest;
    Serial.printf("D,%d,%d,%u,%u,%u,%d,%d\n", (int)s.ball_x, (int)s.ball_y,
                  (unsigned)s.score1, (unsigned)s.score2, (unsigned)s.status,
                  (int)s.p1_x, (int)s.p2_x);
  }
    else {
      Serial.println("waiting for status from host...");
    }
  }
}