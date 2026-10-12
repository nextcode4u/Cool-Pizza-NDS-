#ifndef COOL_PIZZA_GAME_H
#define COOL_PIZZA_GAME_H
#include "original.h"
#define MAX_ENEMIES 32
#define MAX_PROJECTILES 48
#define TOTAL_WAVES 42
typedef enum { TITLE, PLAYING, PAUSED, DEAD, FINISHED, SCORES, INTRO } Phase;
typedef struct {
  int active, config, stage, hp, clip, clip_tick, hurt, attack_age,
      attack_state;
  float time, life, x, y, sx, sy, angle, target_x, base_x, base_y;
} Enemy;
typedef struct {
  int active, kind, age;
  float x, y, vx, vy;
} Projectile;
typedef struct {
  int active, warning, used;
  float time;
} Ramp;
typedef struct {
  Phase phase;
  int frame, wave, clock, score, combo, best, plays, steer, attack,
      attack_pending, attack_connected, wave_count;
  int clip, clip_tick, kill_boost_wait, hit_boost_wait, shield;
  int power, level, progress, power_time, nuke_flash, dead_clock, muted;
  int sequence[TOTAL_WAVES + 1], wave_id, ramp_started, next_ramps, transition;
  unsigned rng, sounds;
  float x, y, vy, camera_y, ghost_x;
  uint64_t previous_contacts[2], contacts[2];
  int intro_stage, intro_clock;
  Enemy enemies[MAX_ENEMIES];
  Projectile projectiles[MAX_PROJECTILES];
  Ramp ramps[6];
} Game;
typedef struct {
  int steer, strike, start, power, back, scores, mute;
} Input;
void game_init(Game *g);
void game_start(Game *g, unsigned seed);
void game_enter_wave(Game *g, int wave_id);
void game_step(Game *g, Input input);
void game_render(const Game *g, unsigned short *pixels);
void game_render_bottom(const Game *g, unsigned short *pixels);
MotionSample motion_sample(int id, float time, int repeat);
int animation_frame(int clip, int ticks);
int game_can_attack(const Game *g);
int game_enemy_sprite(const Enemy *e);
#endif
