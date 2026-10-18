#pragma once
#include <stdint.h>

enum PacketType : uint8_t { PKT_INPUT = 1, PKT_STATE = 2 };
enum GameStatus : uint8_t { ST_SERVE = 0, ST_PLAYING = 1, ST_GAME_OVER = 2 };

#pragma pack(push, 1)
// client to host (9 B)
struct InputPacket {
  uint8_t  type;      // PKT_INPUT
  uint16_t seq;       // sequence number (loss packet counter)
  int16_t  paddle_x;  // middle of the paddle - player 2
  uint32_t t_ms;      // transmitters millis()
};

// host to client (and then phone) (18 B)
struct StatePacket {
  uint8_t  type;      // PKT_STATE
  uint16_t seq;
  int16_t  ball_x, ball_y;
  int16_t  p1_x, p2_x;     // paddle 1 (down, host), paddle 2 (up, client)
  uint8_t  score1, score2;
  uint8_t  status;         // GameStatus
  uint32_t host_t_ms;      // host millis()
};
#pragma pack(pop)