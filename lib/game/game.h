#pragma once
// game logic - no dependency on Arduino.h, so it can be tested on PC)
#include <stdint.h>
#include "protocol.h"

namespace game {

constexpr int16_t FIELD_W = 400;
constexpr int16_t FIELD_H = 600;
constexpr int16_t PADDLE_W = 80;
constexpr int16_t PADDLE_H = 10;
constexpr int16_t PADDLE1_Y = FIELD_H - 30;  // top of paddle 1 (bottom of the board)
constexpr int16_t PADDLE2_Y = 20;            // top of paddle 2 (top of the board)
constexpr int16_t BALL_R = 6;
constexpr uint8_t WIN_SCORE = 5;

constexpr float PADDLE_SPEED = 300.0f; 
constexpr float BALL_SPEED0 = 220.0f;
constexpr float BALL_SPEED_MAX = 520.0f;

struct Game {
  float bx, by, vx, vy;
  float p1x, p2x;
  uint8_t score1, score2;
  GameStatus status;
  float serveTimer;
  uint32_t rng;
};

void init(Game& g);
void restart(Game& g);
void setPaddle1(Game& g, float x);
void setPaddle2(Game& g, float x);
float clampPaddle(float x);
void step(Game& g, float dt);                 // dt [seconds]
void toPacket(const Game& g, StatePacket& p); // fills the states (type/seq/time are called)

}  // namespace game