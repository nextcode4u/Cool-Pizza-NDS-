#ifndef COOLPIZZA_ORIGINAL_H
#define COOLPIZZA_ORIGINAL_H
#include <stdint.h>
typedef struct {
  float x, y, sx, sy, angle;
} MotionSample;
typedef struct {
  float time;
} MotionEvent;
typedef struct {
  const MotionSample *samples;
  const MotionEvent *events;
  int count, event_count, wrap, flags;
  float duration;
} Motion;
typedef struct {
  const char *name;
  const short *frames;
  int count, loop_start, wrap;
  float fps;
} SpriteClip;
typedef struct {
  int width, height;
  float x0, y0, world_w, world_h;
  const uint16_t *pixels;
} Sprite;
typedef struct {
  int sprite, advance, offset_y;
} Glyph;
extern const Glyph original_font[128];
typedef struct {
  const float *xy;
  int count, loop, ease;
} PathData;
typedef struct {
  float px, py, psx, psy, x, y, sx, sy, cx, cy, hx, hy;
  float speed, lifetime, tracking, charge, active, tracking_speed, tell, impact,
      return_time, pull_air, pull_ground;
  float path_origin_x, path_origin_y, path_scale_x, path_scale_y;
  float bomb_x, bomb_y, laser_start_y;
  int path_loop, path_ease;
  int kind, hp, value, idle, attack, entrance, motion, exit, path, attack_sound,
      death_sound;
} EnemyConfig;
typedef struct {
  const char *name;
  int first, count;
  float ramps, life, delay, alert;
  uint64_t formations;
  int ramp_only;
} WaveConfig;
typedef struct {
  float x, y, sx, sy, hx, hy;
  int motion, sprite, visual_motion, animation;
} RampConfig;
typedef struct {
  float x, y, sx, sy;
  int motion, sprite;
} Scenery;
extern const Motion original_motions[];
extern const SpriteClip original_clips[];
extern const Sprite original_sprites[];
extern const PathData original_paths[];
extern const EnemyConfig original_enemies[];
extern const WaveConfig original_waves[];
extern const RampConfig original_ramps[6];
extern const Scenery original_scenery[];
extern const int original_motion_count, original_clip_count,
    original_sprite_count, original_wave_count, original_enemy_count,
    original_scenery_count;
extern const int original_opening[3], original_pool1[], original_pool2[],
    original_pool3[], original_fixed[5];
extern const int original_pool_sizes[3];
extern const int tutorial_wave;
extern const int player_clips[6], bomb_clip, ghost_clip, shield_clip,
    reticle_clip;
extern const int art_logo, art_start, art_scores, art_dead, art_win,
    art_power[4], art_meter;
extern const float player_attack_box[4], player_board_box[4];
extern const int ramp_alert_clip;
extern const int art_company, art_tettix;
#endif
