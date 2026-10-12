#include "game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void tick(Game *g, int count) {
  for (int i = 0; i < count; i++)
    game_step(g, (Input){0});
}
static void physics(void) {
  Game g;
  game_init(&g);
  g.plays = 3;
  game_start(&g, 17);
  g.ramp_started = 1;
  g.next_ramps = 100000;
  g.vy = 4;
  float peak = g.y;
  for (int i = 0; i < 100; i++) {
    game_step(&g, (Input){0});
    if (g.y > peak)
      peak = g.y;
    assert(isfinite(g.y) && isfinite(g.camera_y));
  }
  assert(fabsf(peak - 1.0166667f) < .025f);
  assert(g.y == -.55f && !game_can_attack(&g));
  game_step(&g, (Input){.strike = 1});
  assert(g.attack == 0);
  g.y = 0;
  g.vy = 1;
  game_step(&g, (Input){.strike = 1});
  assert(g.attack == 6);
  Game before = g;
  game_step(&g, (Input){.start = 1});
  assert(g.phase == PAUSED);
  Game pause = g;
  game_step(&g, (Input){.steer = 1, .strike = 1});
  assert(memcmp(&pause, &g, sizeof g) == 0);
  game_step(&g, (Input){.start = 1});
  assert(g.phase == PLAYING && g.frame == before.frame);
}
static void data(void) {
  assert(original_wave_count == 47 && original_enemy_count == 196);
  assert(original_clip_count == 25 && original_motion_count == 47);
  for (int i = 0; i < original_wave_count; i++) {
    const WaveConfig *w = &original_waves[i];
    assert(w->count <= MAX_ENEMIES &&
           w->first + w->count <= original_enemy_count);
  }
  for (int i = 0; i < original_enemy_count; i++) {
    const EnemyConfig *c = &original_enemies[i];
    assert(c->hp > 0 && c->value > 0 && c->idle >= 0 &&
           c->idle < original_clip_count);
  }
  int fly = -1;
  for (int i = 0; i < original_clip_count; i++)
    if (original_clips[i].count == 4 && original_clips[i].fps == 30)
      fly = i;
  assert(fly >= 0 && animation_frame(fly, 0) != animation_frame(fly, 2));
  for (int i = 0; i < original_clip_count; i++)
    for (int t = 0; t < 1000; t += 3)
      assert(animation_frame(i, t) >= 0 &&
             animation_frame(i, t) < original_sprite_count);
  for (int i = 0; i < original_motion_count; i++)
    for (int t = 0; t < 1500; t += 7) {
      MotionSample m = motion_sample(i, t / 60.f, 1);
      assert(isfinite(m.x) && isfinite(m.y) && isfinite(m.sx) &&
             isfinite(m.sy));
    }
}
static void progression(void) {
  Game g;
  game_init(&g);
  game_start(&g, 123);
  assert(g.wave_count == 43 && g.sequence[0] == tutorial_wave &&
         g.sequence[1] == original_opening[0]);
  g.plays = 3;
  game_start(&g, 123);
  assert(g.wave_count == 42 && g.sequence[0] == original_opening[0]);
  int sequence[42];
  memcpy(sequence, g.sequence, sizeof sequence);
  game_start(&g, 124);
  assert(memcmp(sequence, g.sequence, sizeof sequence) != 0);
  unsigned short fb[256 * 192];
  for (int t = 0; t < 5000 && g.phase == PLAYING; t++) {
    /* Force each enemy cleared to audit progression, not player
     * skill/difficulty. */
    for (int i = 0; i < MAX_ENEMIES; i++)
      g.enemies[i].active = 0;
    tick(&g, 1);
    if (t % 17 == 0) {
      game_render(&g, fb);
      game_render_bottom(&g, fb);
    }
  }
  assert(g.phase == FINISHED && g.wave == 42);
  game_step(&g, (Input){.strike = 1});
  assert(g.phase == PLAYING && g.score == 0);
}
static void entrance_frame(void) {
  Game g;
  unsigned short fb[256 * 192];
  int entrances = 0, spawned = 0;
  game_init(&g);
  game_start(&g, 17);
  for (int wave = 0; wave < original_wave_count; wave++) {
    /* Exercise the real end-of-tick transition: rendering happens before the
       newly created enemies receive their first update. */
    memset(g.enemies, 0, sizeof g.enemies);
    memset(g.projectiles, 0, sizeof g.projectiles);
    g.phase = PLAYING;
    g.wave = 1;
    g.wave_count = 2;
    g.sequence[1] = wave;
    g.transition = 1;
    game_step(&g, (Input){0});
    assert(g.wave_id == wave && g.clock == 0);
    for (int i = 0; i < original_waves[wave].count; i++) {
      const Enemy *e = &g.enemies[i];
      const EnemyConfig *c = &original_enemies[e->config];
      assert(e->active && e->config == original_waves[wave].first + i);
      spawned++;
      if (c->entrance < 0) {
        assert(e->stage == 1);
        continue;
      }
      entrances++;
      const Motion *m = &original_motions[c->entrance];
      const MotionSample *first = &m->samples[0];
      assert(e->stage == 0 && e->time == 0 && e->clip_tick == 0);
      if (m->flags & 1) {
        assert(fabsf(e->x - (c->px + first->x * c->psx)) < .00001f);
        assert(fabsf(e->y - (c->py + first->y * c->psy)) < .00001f);
      }
      if (m->flags & 2) {
        assert(fabsf(e->sx - first->sx * c->psx) < .00001f);
        assert(fabsf(e->sy - first->sy * c->psy) < .00001f);
      }
    }
    game_render(&g, fb);
  }
  assert(spawned == original_enemy_count && entrances > 0);
}
static void combat(void) {
  Game g;
  game_init(&g);
  game_start(&g, 99);
  g.ramp_started = 1;
  g.next_ramps = 100000;
  int config = -1;
  for (int i = 0; i < original_enemy_count; i++)
    if (original_enemies[i].hp == 4 && original_enemies[i].kind == 2) {
      config = i;
      break;
    }
  assert(config >= 0);
  memset(g.enemies, 0, sizeof g.enemies);
  const EnemyConfig *c = &original_enemies[config];
  Enemy *e = &g.enemies[0];
  *e = (Enemy){.active = 1,
               .stage = 1,
               .config = config,
               .hp = 4,
               .clip = c->idle,
               .attack_age = -1};
  /* Position player at the authored motion's collider and strike once. */
  tick(&g, 1);
  g.x = e->x + c->cx * e->sx;
  g.y = e->y + c->cy * e->sy - player_attack_box[1];
  g.vy = 0;
  game_step(&g, (Input){.strike = 1});
  assert(e->hp == 3 && g.score == 0);
  for (int h = 0; h < 3; h++) {
    g.attack = 0;
    g.attack_pending = 0;
    e->hurt = 0;
    g.x = e->x + c->cx * e->sx;
    g.y = e->y + c->cy * e->sy - player_attack_box[1];
    g.vy = 0;
    game_step(&g, (Input){.strike = 1});
  }
  assert(e->hp == 0 && g.combo == 1 && g.score == c->value && g.vy > 0);
  /* Grounded hazard death and a shield absorbing exactly one hit. */
  memset(g.enemies, 0, sizeof g.enemies);
  memset(g.projectiles, 0, sizeof g.projectiles);
  g.y = -.55f;
  g.vy = 0;
  g.x = 0;
  g.shield = 1;
  g.projectiles[0] = (Projectile){.active = 1, .kind = 1, .x = 0, .y = -.8f};
  tick(&g, 1);
  assert(g.phase == PLAYING && g.shield == 0);
  tick(&g, 1);
  assert(g.phase == PLAYING);
  /* Persistent overlap is one Unity OnTriggerEnter, not a hit every tick. */
  tick(&g, 3);
  assert(g.phase == PLAYING);
  g.projectiles[1] = (Projectile){.active = 1, .kind = 1, .x = 0, .y = -.8f};
  tick(&g, 1);
  assert(g.phase == DEAD);
}
int main(void) {
  data();
  physics();
  progression();
  entrance_frame();
  combat();
  Game g;
  unsigned short fb[256 * 192];
  unsigned seed = 2;
  game_init(&g);
  game_start(&g, 17);
  for (int i = 0; i < 15000; i++) {
    seed = 1664525u * seed + 1013904223u;
    game_step(&g, (Input){.steer = (int)(seed % 3) - 1,
                          .strike = i % 9 == 0,
                          .power = i % 133 == 0});
    if (g.phase == DEAD)
      game_start(&g, seed);
    if (i % 33 == 0) {
      game_render(&g, fb);
      game_render_bottom(&g, fb);
    }
    assert(isfinite(g.x) && isfinite(g.y) && g.y >= -.55f);
  }
  puts("Recovered data, animation, physics, combat, shield, pause and "
       "progression checks passed.");
}
