#include "game.h"
#include <math.h>

namespace game {

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float clampPaddle(float x) { return clampf(x, PADDLE_W / 2.0f, FIELD_W - PADDLE_W / 2.0f); }

// dir = +1: the ball goes toward the paddle 1 (down), -1: towards paddle 2
static void serve(Game& g, int dir) {
  g.rng = g.rng * 1664525u + 1013904223u;
  g.bx = FIELD_W / 2.0f;
  g.by = FIELD_H / 2.0f;
  g.vx = (float)((int)((g.rng >> 16) % 101) - 50);
  g.vy = dir * BALL_SPEED0;
  g.serveTimer = 1.0f;
  g.status = ST_SERVE;
}

void init(Game& g) {
  g.rng = 12345;
  restart(g);
}

void restart(Game& g) {
  g.score1 = g.score2 = 0;
  g.p1x = g.p2x = FIELD_W / 2.0f;
  serve(g, 1);
}

void setPaddle1(Game& g, float x) { g.p1x = clampPaddle(x); }
void setPaddle2(Game& g, float x) { g.p2x = clampPaddle(x); }

void step(Game& g, float dt) {
  if (g.status == ST_GAME_OVER) return;
  if (g.status == ST_SERVE) {
    g.serveTimer -= dt;
    if (g.serveTimer <= 0) g.status = ST_PLAYING;
    return;
  }

  g.bx += g.vx * dt;
  g.by += g.vy * dt;

  // side walls
  if (g.bx < BALL_R)           { g.bx = BALL_R;           g.vx = fabsf(g.vx); }
  if (g.bx > FIELD_W - BALL_R) { g.bx = FIELD_W - BALL_R; g.vx = -fabsf(g.vx); }

  // paddle 1 (down)
  if (g.vy > 0 && g.by + BALL_R >= PADDLE1_Y && g.by - BALL_R <= PADDLE1_Y + PADDLE_H &&
      fabsf(g.bx - g.p1x) <= PADDLE_W / 2.0f + BALL_R) {
    g.by = PADDLE1_Y - BALL_R;
    g.vy = -clampf(g.vy * 1.05f, 0, BALL_SPEED_MAX);
    g.vx = clampf(g.vx + (g.bx - g.p1x) * 4.0f, -BALL_SPEED_MAX * 0.8f, BALL_SPEED_MAX * 0.8f);
  }
  // paddle 2 (up)
  const float p2bottom = PADDLE2_Y + PADDLE_H;
  if (g.vy < 0 && g.by - BALL_R <= p2bottom && g.by + BALL_R >= PADDLE2_Y &&
      fabsf(g.bx - g.p2x) <= PADDLE_W / 2.0f + BALL_R) {
    g.by = p2bottom + BALL_R;
    g.vy = clampf(-g.vy * 1.05f, 0, BALL_SPEED_MAX);
    g.vx = clampf(g.vx + (g.bx - g.p2x) * 4.0f, -BALL_SPEED_MAX * 0.8f, BALL_SPEED_MAX * 0.8f);
  }

  // score
  if (g.by > FIELD_H) {          // player 1 lost a point
    g.score2++;
    if (g.score2 >= WIN_SCORE) g.status = ST_GAME_OVER; else serve(g, 1);
  } else if (g.by < 0) {         // player 2 lost a point
    g.score1++;
    if (g.score1 >= WIN_SCORE) g.status = ST_GAME_OVER; else serve(g, -1);
  }
}

void toPacket(const Game& g, StatePacket& p) {
  p.ball_x = (int16_t)g.bx;
  p.ball_y = (int16_t)g.by;
  p.p1_x = (int16_t)g.p1x;
  p.p2_x = (int16_t)g.p2x;
  p.score1 = g.score1;
  p.score2 = g.score2;
  p.status = g.status;
}

}  // namespace game