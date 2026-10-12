#include "game.h"
#include <math.h>
#include <string.h>
#define DT (1.0f / 60.0f)
#define GROUND (-0.55f)
static unsigned random_number(Game *g) {
  g->rng = g->rng * 1664525u + 1013904223u;
  return g->rng;
}
static float approach(float x, float target, float speed) {
  return x < target ? fminf(x + speed, target) : fmaxf(x - speed, target);
}
static void sound(Game *g, int id) {
  if (id >= 126 && id <= 150)
    g->sounds |= 1u << (id - 126);
}
int game_can_attack(const Game *g) { return g->y > GROUND; }
MotionSample motion_sample(int id, float time, int repeat) {
  MotionSample blank = {0, 0, 1, 1, 0};
  if (id < 0 || id >= original_motion_count)
    return blank;
  const Motion *m = &original_motions[id];
  if (m->duration > 0 && time > m->duration) {
    if (repeat && m->wrap == 2)
      time = fmodf(time, m->duration);
    else if (repeat && m->wrap == 4) {
      time = fmodf(time, 2 * m->duration);
      if (time > m->duration)
        time = 2 * m->duration - time;
    } else
      time = m->duration;
  }
  float p = fmaxf(time, 0) * 60;
  int a = (int)p;
  if (a >= m->count - 1)
    return m->samples[m->count - 1];
  float u = p - a;
  const MotionSample *x = &m->samples[a], *y = &m->samples[a + 1];
  return (MotionSample){x->x + (y->x - x->x) * u, x->y + (y->y - x->y) * u,
                        x->sx + (y->sx - x->sx) * u,
                        x->sy + (y->sy - x->sy) * u,
                        x->angle + (y->angle - x->angle) * u};
}
int animation_frame(int id, int ticks) {
  if (id < 0 || id >= original_clip_count)
    return 0;
  const SpriteClip *c = &original_clips[id];
  int f = (int)(ticks * c->fps / 60);
  if (c->count <= 0)
    return 0;
  if (c->wrap == 0)
    f %= c->count;
  else if (c->wrap == 1 && f >= c->count) {
    int start = c->loop_start;
    if (start < 0 || start >= c->count)
      start = c->count - 1;
    f = start + (f - start) % (c->count - start);
  } else if (c->wrap == 3 && c->count > 1) {
    f %= 2 * c->count - 2;
    if (f >= c->count)
      f = 2 * c->count - 2 - f;
  } else if (f >= c->count)
    f = c->count - 1;
  return c->frames[f];
}
int game_enemy_sprite(const Enemy *e) {
  return animation_frame(e->clip, e->clip_tick);
}
static void enemy_motion(Enemy *e, float time, int id);
void game_init(Game *g) {
  memset(g, 0, sizeof(*g));
  g->phase = INTRO;
  g->rng = 17;
  g->y = GROUND;
  g->clip = player_clips[0];
  g->wave = 1;
  g->wave_count = TOTAL_WAVES;
}
void game_enter_wave(Game *g, int id) {
  if (id < 0 || id >= original_wave_count)
    return;
  g->wave_id = id;
  g->clock = 0;
  g->ramp_started = 0;
  g->transition = -1;
  memset(g->enemies, 0, sizeof(g->enemies));
  g->previous_contacts[0] &= ~0xffffffffULL;
  memset(g->ramps, 0, sizeof(g->ramps));
  const WaveConfig *w = &original_waves[id];
  for (int i = 0; i < w->count && i < MAX_ENEMIES; i++) {
    int ci = w->first + i;
    const EnemyConfig *c = &original_enemies[ci];
    Enemy *e = &g->enemies[i];
    *e = (Enemy){.active = 1,
                 .config = ci,
                 .hp = c->hp,
                 .clip = c->idle,
                 .attack_age = -1};
    e->x = c->px + c->x * c->psx;
    e->y = c->py + c->y * c->psy;
    e->sx = c->sx * c->psx;
    e->sy = c->sy * c->psy;
    if (c->entrance < 0)
      e->stage = 1;
    else
      /* A wave can be rendered immediately after the transition, before its
         first update. Start at the entrance pose rather than the scene pose. */
      enemy_motion(e, 0, c->entrance);
  }
}
static void shuffled(Game *g, const int *pool, int n, int take, int *offset) {
  int p[32];
  for (int i = 0; i < n; i++)
    p[i] = pool[i];
  for (int i = n - 1; i > 0; i--) {
    int j = (int)(random_number(g) % (unsigned)(i + 1));
    int t = p[i];
    p[i] = p[j];
    p[j] = t;
  }
  for (int i = 0; i < take; i++)
    g->sequence[(*offset)++] = p[i];
}
void game_start(Game *g, unsigned seed) {
  int best = g->best, plays = g->plays + 1, muted = g->muted;
  int frame = g->frame;
  game_init(g);
  g->best = best;
  g->plays = plays;
  g->muted = muted;
  g->rng = seed;
  g->frame = frame + 1;
  int n = 0;
  if (plays <= 3)
    g->sequence[n++] = tutorial_wave;
  for (int i = 0; i < 3; i++)
    g->sequence[n++] = original_opening[i];
  shuffled(g, original_pool1, original_pool_sizes[0], 10, &n);
  g->sequence[n++] = original_fixed[0];
  shuffled(g, original_pool2, original_pool_sizes[1], 16, &n);
  shuffled(g, original_pool3, original_pool_sizes[2], 8, &n);
  for (int i = 1; i < 5; i++)
    g->sequence[n++] = original_fixed[i];
  g->wave_count = n;
  g->phase = PLAYING;
  game_enter_wave(g, g->sequence[0]);
}
static int overlap(float ax, float ay, float hx, float hy, float bx, float by,
                   float bhx, float bhy) {
  return fabsf(ax - bx) <= hx + bhx && fabsf(ay - by) <= hy + bhy;
}
static void set_player_clip(Game *g, int c) {
  if (g->clip != c) {
    g->clip = c;
    g->clip_tick = 0;
  }
}
static void damage_player(Game *g) {
  if (g->phase != PLAYING)
    return;
  if (g->shield) {
    g->shield = 0;
    g->power_time = 0;
    g->power = 0;
    sound(g, 136);
    return;
  }
  g->phase = DEAD;
  g->dead_clock = 0;
  g->attack = 0;
  if (g->score > g->best)
    g->best = g->score;
}
static void damage_contact(Game *g, unsigned id, int touching) {
  if (!touching)
    return;
  unsigned slot = id / 64;
  uint64_t bit = 1ULL << (id % 64);
  g->contacts[slot] |= bit;
  if (!(g->previous_contacts[slot] & bit))
    damage_player(g);
}
static void kill_enemy(Game *g, Enemy *e, int awarded) {
  if (!e->active || e->stage == 3)
    return;
  const EnemyConfig *c = &original_enemies[e->config];
  e->stage = 3;
  e->life = 0;
  e->hp = 0;
  e->attack_age = -1;
  sound(g, c->death_sound);
  if (!awarded)
    return;
  if (g->power != 3 || !g->power_time)
    g->combo++;
  g->score += c->value * (g->combo ? g->combo : 1);
  if (g->combo > 1 && !g->power_time) {
    sound(g, 142);
    if (++g->progress >= 5) {
      g->progress = 0;
      if (g->level < 4)
        g->level++;
      g->power = g->level;
      sound(g, 139);
      sound(g, 146);
    }
  }
  if (!g->kill_boost_wait) {
    g->vy += 3;
    g->kill_boost_wait = 30;
  }
}
static void hit_enemy(Game *g, Enemy *e, int damage) {
  if (e->stage != 1 || e->hurt)
    return;
  e->hp -= damage;
  e->hurt = 6;
  g->sounds &= ~(1u << (134 - 126));
  sound(g, 126);
  if (g->vy <= 0 && !g->hit_boost_wait) {
    g->vy = 0;
    g->x += e->x < g->x ? 5 * DT : -5 * DT;
    g->hit_boost_wait = 6;
  }
  if (e->hp <= 0)
    kill_enemy(g, e, 1);
}
static void projectile(Game *g, int kind, float x, float y, float vx,
                       float vy) {
  for (int i = 0; i < MAX_PROJECTILES; i++)
    if (!g->projectiles[i].active) {
      g->projectiles[i] = (Projectile){1, kind, 0, x, y, vx, vy};
      return;
    }
}
static void enemy_attack(Game *g, Enemy *e) {
  const EnemyConfig *c = &original_enemies[e->config];
  if (c->kind == 0 || e->attack_age >= 0 ||
      ((c->kind == 1 || c->kind == 2) && game_can_attack(g)))
    return;
  e->attack_age = 0;
  e->attack_state = 1;
  e->clip = c->attack;
  e->clip_tick = 0;
  e->base_x = e->x;
  e->base_y = e->y;
  e->target_x = e->x;
  if (c->kind == 4)
    e->target_x = game_can_attack(g) ? c->pull_air : c->pull_ground;
  sound(g, c->kind == 3 ? 132 : c->kind == 4 ? 144 : c->attack_sound);
}
static void enemy_motion(Enemy *e, float time, int id) {
  const EnemyConfig *c = &original_enemies[e->config];
  MotionSample m = motion_sample(id, time, e->stage == 1);
  float px = c->px, py = c->py;
  if (e->stage == 1 && c->path >= 0 && c->motion >= 0) {
    const PathData *path = &original_paths[c->path];
    float period =
        original_motions[c->motion].duration / fmaxf(c->speed, .001f);
    float t =
        period > 0
            ? fmodf(e->time, period * (c->path_loop == 2 ? 2 : 1)) / period
            : 0;
    if (t > 1)
      t = 2 - t;
    if (c->path_ease == 1) {
      float s = 1.70158f * 1.525f, u = t * 2;
      if (u < 1)
        t = .5f * u * u * ((s + 1) * u - s);
      else {
        u -= 2;
        t = .5f * (u * u * ((s + 1) * u + s) + 2);
      }
    } else if (c->path_ease == 2) {
      if (t < .5f)
        t = 4 * t * t * t;
      else {
        float u = 2 * t - 2;
        t = .5f * u * u * u + 1;
      }
    } else if (c->path_ease == 3)
      t = t * (2 - t);
    else if (c->path_ease == 4)
      t *= t;
    t = fmaxf(0, fminf(1, t));
    float p = t * (path->count - 1);
    int a = (int)p;
    float u = p - a;
    if (a >= path->count - 1)
      a = path->count - 2;
    px += (path->xy[a * 2] + (path->xy[a * 2 + 2] - path->xy[a * 2]) * u) *
              c->path_scale_x -
          c->path_origin_x;
    py += (path->xy[a * 2 + 1] +
           (path->xy[a * 2 + 3] - path->xy[a * 2 + 1]) * u) *
              c->path_scale_y -
          c->path_origin_y;
  }
  int flags = id >= 0 ? original_motions[id].flags : 0;
  e->x = px + ((flags & 1) ? m.x : c->x) * c->psx;
  e->y = py + ((flags & 1) ? m.y : c->y) * c->psy;
  e->sx = c->psx * ((flags & 2) ? m.sx : c->sx);
  e->sy = c->psy * ((flags & 2) ? m.sy : c->sy);
  e->angle = m.angle;
}
static void update_enemy(Game *g, Enemy *e) {
  const EnemyConfig *c = &original_enemies[e->config];
  e->clip_tick++;
  if (e->hurt)
    e->hurt--;
  if (e->stage == 3) {
    e->life += DT;
    if (e->life > .3f)
      e->active = 0;
    return;
  }
  float previous = e->time;
  e->time += DT;
  int id = e->stage == 0 ? c->entrance : e->stage == 2 ? c->exit : c->motion;
  float speed = e->stage == 1 ? c->speed : 1;
  enemy_motion(e, e->time * speed, id);
  if (c->kind == 0) {
    /* CoinMonster.IdleAnimation: the original figure-eight hover. */
    float t = g->frame * DT * 5, s = sinf(t),
          x = .141421356f * cosf(t) / (s * s + 1);
    e->x += x;
    e->y += x * s;
  }
  if (c->kind == 2) {
    e->x += cosf(g->frame * DT * 2) / 10;
    e->y += sinf(g->frame * DT * 2) / 10;
  }
  if (e->stage == 0 && (id < 0 || e->time >= original_motions[id].duration)) {
    e->stage = 1;
    e->time = 0;
    e->life = 0;
  } else if (e->stage == 2 &&
             (id < 0 || e->time >= original_motions[id].duration)) {
    e->active = 0;
    return;
  } else if (e->stage == 1) {
    e->life += DT;
    if (c->lifetime >= 0 && e->life >= c->lifetime) {
      e->stage = 2;
      e->time = 0;
      e->attack_age = -1;
      return;
    }
    if (c->motion >= 0) {
      const Motion *m = &original_motions[c->motion];
      for (int i = 0; i < m->event_count; i++) {
        float a = previous * speed, b = e->time * speed, t = m->events[i].time;
        if (m->wrap == 2 && m->duration > 0) {
          int lap = (int)(b / m->duration);
          t += lap * m->duration;
          if (t > b)
            t -= m->duration;
        }
        if (t >= a - .00001f && t < b + .00001f && (a == 0 || t > a + .00001f))
          enemy_attack(g, e);
      }
    }
  }
  if (e->attack_age < 0)
    return;
  int age = e->attack_age++;
  float t = age * DT;
  if (c->kind == 1) {
    if (age == 13)
      projectile(g, 0, e->x + c->bomb_x * e->sx, e->y + c->bomb_y * e->sy, 0,
                 -2.5f);
    if (t >= original_clips[c->attack].count / original_clips[c->attack].fps) {
      e->attack_age = -1;
      e->clip = c->idle;
      e->clip_tick = 0;
    }
  } else if (c->kind == 2) {
    if (t < c->tracking)
      e->target_x = approach(e->target_x, g->x, c->tracking_speed * DT);
    else if (t >= c->tracking + c->charge &&
             t < c->tracking + c->charge + c->active) {
      e->attack_state = 2;
      if (age == (int)((c->tracking + c->charge) * 60))
        sound(g, 131);
      damage_contact(
          g, (unsigned)(e - g->enemies),
          overlap(g->x, g->y, .123f, .33f, e->target_x, -.8f, .1f, .125f));
    } else if (t >= c->tracking + c->charge + c->active) {
      e->attack_age = -1;
      e->attack_state = 0;
      e->clip = c->idle;
      e->clip_tick = 0;
    }
  } else if (c->kind == 3) {
    float animation =
        original_clips[c->attack].count / original_clips[c->attack].fps;
    float fall = animation + c->tell;
    if (t < animation) { /* pose precedes the tell */
    } else if (t < fall)
      e->y = e->base_y + .25f * (t - animation) / fmaxf(c->tell, .01f);
    else if (t < fall + c->impact) {
      float u = (t - fall) / fmaxf(c->impact, .01f);
      e->y = e->base_y + .25f + (-.55f - e->base_y - .25f) * u;
    } else if (t < fall + c->impact + .25f) {
      e->y = -.55f;
      damage_contact(g, (unsigned)(e - g->enemies),
                     overlap(g->x, g->y, .123f, .33f, e->x, e->y, .2f, .2f));
      if (age == (int)((fall + c->impact) * 60))
        sound(g, 141);
    } else if (t < fall + c->impact + .25f + animation) {
      e->y = -.55f;
      if (e->attack_state != 2) {
        e->attack_state = 2;
        e->clip = c->attack + 1;
        e->clip_tick = 0;
      }
    } else if (t < fall + c->impact + .25f + animation + c->return_time) {
      float u = (t - fall - c->impact - .25f - animation) /
                fmaxf(c->return_time, .01f);
      e->y = -.55f + (c->py + c->y * c->psy + .55f) * u;
      if (e->clip != c->idle) {
        e->clip = c->idle;
        e->clip_tick = 0;
      }
    } else {
      e->attack_age = -1;
      e->attack_state = 0;
      e->clip = c->idle;
      e->clip_tick = 0;
    }
  } else if (c->kind == 4) {
    if (t < 2)
      g->x = approach(g->x, e->x, e->target_x * DT);
    else if (t < 5.5f) {
      e->attack_state = 2;
      if (age == 120) {
        e->base_y = e->y + c->laser_start_y * e->sy;
        sound(g, 129);
      }
      float blast_y = fmaxf(-.8f, e->base_y - (age - 120) * .1f);
      damage_contact(g, (unsigned)(e - g->enemies),
                     overlap(g->x, g->y, .123f, .33f, e->x, blast_y,
                             .5f * fabsf(e->sx), .1f * fabsf(e->sy)));
    } else {
      e->attack_age = -1;
      e->attack_state = 0;
      e->clip = c->idle;
      e->clip_tick = 0;
    }
  }
}
static void start_ramps(Game *g) {
  const WaveConfig *w = &original_waves[g->wave_id];
  int choices[64], n = 0;
  for (int i = 0; i < 64; i++)
    if (w->formations & (1ULL << i))
      choices[n++] = i;
  if (!n)
    return;
  int mask = choices[random_number(g) % (unsigned)n];
  for (int i = 0; i < 6; i++)
    if (mask & (1 << i))
      g->ramps[i] = (Ramp){1, (int)(w->alert * 60), 0, 0};
}
static void activate_power(Game *g) {
  if (!g->power || g->power_time)
    return;
  g->power_time = g->power == 1   ? 90
                  : g->power == 2 ? 60
                  : g->power == 3 ? 30
                                  : -1;
  g->level = 0;
  g->progress = 0;
  if (g->power == 1) {
    g->vy = 4;
    sound(g, 145);
  } else if (g->power == 2) {
    projectile(g, 2, g->ghost_x, GROUND, 0, 4);
    sound(g, 146);
  } else if (g->power == 3) {
    g->nuke_flash = 30;
    sound(g, 128);
    for (int i = 0; i < MAX_ENEMIES; i++)
      if (g->enemies[i].active)
        kill_enemy(g, &g->enemies[i], 1);
  } else {
    g->shield = 1;
    sound(g, 136);
  }
}
void game_step(Game *g, Input in) {
  g->sounds = 0;
  if (g->phase == INTRO) {
    g->frame++;
    g->intro_clock++;
    if (g->intro_clock >= 180 || in.strike || in.start || in.back) {
      g->intro_clock = 0;
      if (++g->intro_stage == 2)
        g->phase = TITLE;
    }
    return;
  }
  if (in.mute)
    g->muted = !g->muted;
  if (g->phase == TITLE || g->phase == SCORES || g->phase == DEAD ||
      g->phase == FINISHED) {
    g->frame++;
    g->clip_tick++;
    if (g->phase == DEAD)
      g->dead_clock++;
    if (in.scores)
      g->phase = SCORES;
    if (in.back)
      g->phase = TITLE;
    if (in.start || in.strike)
      game_start(g, g->rng + (unsigned)g->frame);
    return;
  }
  if (in.back) {
    g->phase = TITLE;
    return;
  }
  if (in.start) {
    g->phase = g->phase == PAUSED ? PLAYING : PAUSED;
    return;
  }
  if (g->phase != PLAYING)
    return;
  g->frame++;
  g->clock++;
  g->contacts[0] = g->contacts[1] = 0;
  g->clip_tick++;
  if (g->attack)
    g->attack--;
  if (g->kill_boost_wait)
    g->kill_boost_wait--;
  if (g->hit_boost_wait)
    g->hit_boost_wait--;
  if (g->power_time > 0 && --g->power_time == 0)
    g->power = 0;
  if (g->nuke_flash)
    g->nuke_flash--;
  g->steer = in.steer;
  float move = 10 * DT * (game_can_attack(g) ? .3f : 1);
  if (!g->hit_boost_wait)
    g->x += in.steer * fminf(move, .1f);
  g->x = fmaxf(-1.35f, fminf(1.35f, g->x));
  float previous_y = g->y;
  g->vy -= 5 * DT;
  g->vy = fminf(g->vy, 4);
  g->y += g->vy * DT;
  if (g->y < GROUND) {
    if (previous_y > GROUND)
      sound(g, 150);
    g->y = GROUND;
    g->combo = 0;
  }
  if (game_can_attack(g) && in.strike)
    g->attack_pending = 1;
  if (g->attack_pending && !g->attack && game_can_attack(g)) {
    g->attack_pending = 0;
    g->attack = 6;
    g->attack_connected = 0;
    sound(g, 134);
    g->clip = (random_number(g) & 1) ? player_clips[3] : player_clips[4];
    g->clip_tick = 0;
  }
  if (!g->attack) {
    if (!game_can_attack(g))
      set_player_clip(g, player_clips[in.steer < 0 ? 1 : in.steer > 0 ? 2 : 0]);
    else if (g->clip != player_clips[3] && g->clip != player_clips[4])
      set_player_clip(g, player_clips[5]);
  }
  if (in.power || (!game_can_attack(g) && in.strike))
    activate_power(g);
  g->ghost_x = approach(g->ghost_x, g->x + .25f, 2 * DT);
  g->camera_y = approach(g->camera_y, game_can_attack(g) ? g->y : 0, 4 * DT);
  const WaveConfig *w = &original_waves[g->wave_id];
  int entering = 0, alive = 0;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    Enemy *e = &g->enemies[i];
    if (!e->active)
      continue;
    update_enemy(g, e);
    if (e->active && e->stage != 3)
      alive++;
    if (e->active && e->stage == 0)
      entering++;
    if (e->stage != 1 || !e->active)
      continue;
    const EnemyConfig *c = &original_enemies[e->config];
    if (g->attack && !g->attack_connected && !e->hurt &&
        overlap(g->x + player_attack_box[0], g->y + player_attack_box[1],
                player_attack_box[2], player_attack_box[3],
                e->x + c->cx * e->sx, e->y + c->cy * e->sy,
                c->hx * fabsf(e->sx), c->hy * fabsf(e->sy))) {
      hit_enemy(g, e, 1);
      g->attack_connected = 1;
    }
  }
  if (!entering && !g->ramp_started) {
    g->ramp_started = 1;
    g->next_ramps = g->clock;
  }
  if (g->ramp_started && w->ramps > 0 && g->clock >= g->next_ramps) {
    start_ramps(g);
    g->next_ramps = g->clock + (int)(w->ramps * 60);
  }
  for (int i = 0; i < 6; i++) {
    Ramp *r = &g->ramps[i];
    if (!r->active)
      continue;
    if (r->warning > 0) {
      r->warning--;
      continue;
    }
    r->time += DT;
    const RampConfig *c = &original_ramps[i];
    MotionSample m = motion_sample(c->motion, r->time, 0);
    if (!r->used && g->y <= -.4f &&
        overlap(g->x + player_board_box[0], g->y + player_board_box[1],
                player_board_box[2], player_board_box[3], c->x + m.x,
                c->y + m.y, c->hx, c->hy)) {
      g->vy = 4;
      r->used = 1;
      sound(g, 149);
      g->clip = player_clips[5];
      g->clip_tick = 0;
    }
    if (r->time >= original_motions[c->motion].duration)
      r->active = 0;
  }
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    Projectile *p = &g->projectiles[i];
    if (!p->active)
      continue;
    p->age++;
    p->x += p->vx * DT;
    p->y += p->vy * DT;
    if (p->kind == 0 && p->y <= -.8f) {
      p->y = -.8f;
      p->kind = 1;
      p->age = 0;
      sound(g, 128);
    }
    if (p->kind == 1) {
      damage_contact(g, (unsigned)(32 + i),
                     overlap(g->x, g->y, .123f, .33f, p->x, p->y, .12f, .09f));
      if (p->age >= 15)
        p->active = 0;
    } else if (p->kind == 2) {
      for (int j = 0; j < MAX_ENEMIES; j++) {
        Enemy *e = &g->enemies[j];
        if (e->active && overlap(p->x, p->y, .1f, .1f, e->x, e->y, .2f, .2f))
          hit_enemy(g, e, 4);
      }
    }
    if (p->age > (p->kind == 2 ? 60 : 180))
      p->active = 0;
  }
  g->previous_contacts[0] = g->contacts[0];
  g->previous_contacts[1] = g->contacts[1];
  int complete =
      w->ramp_only ? w->life >= 0 && g->clock >= w->life * 60 : !alive;
  if (complete && g->transition < 0)
    g->transition = (int)(w->delay * 60) + 1;
  if (g->transition > 0 && --g->transition == 0 && g->phase == PLAYING) {
    if (g->wave == g->wave_count) {
      g->phase = FINISHED;
      if (g->score > g->best)
        g->best = g->score;
    } else {
      g->wave++;
      game_enter_wave(g, g->sequence[g->wave - 1]);
    }
  }
}
