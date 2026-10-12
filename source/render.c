#include "game.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { BLACK = 0x8000, WHITE = 0xffff, PINK = 0xd01f, LIME = 0x83fa };
static uint16_t *fb;
static int top = 10, bottom = 181;
static void pixel(int x, int y, uint16_t c) {
  if ((unsigned)x < 256 && y >= top && y < bottom)
    fb[y * 256 + x] = c;
}
static void box(int x, int y, int w, int h, uint16_t c) {
  int x1 = x + w, y1 = y + h;
  if (x < 0)
    x = 0;
  if (y < top)
    y = top;
  if (x1 > 256)
    x1 = 256;
  if (y1 > bottom)
    y1 = bottom;
  for (int j = y; j < y1; j++)
    for (int i = x; i < x1; i++)
      fb[j * 256 + i] = c;
}
static void line(int x, int y, int xx, int yy, uint16_t c) {
  int dx = abs(xx - x), sx = x < xx ? 1 : -1, dy = -abs(yy - y),
      sy = y < yy ? 1 : -1, e = dx + dy;
  for (;;) {
    pixel(x, y, c);
    if (x == xx && y == yy)
      break;
    int ee = 2 * e;
    if (ee >= dy) {
      e += dy;
      x += sx;
    }
    if (ee <= dx) {
      e += dx;
      y += sy;
    }
  }
}
static void sprite_at(int id, int x, int y, int w, int h, int flipx, int flipy,
                      int tint) {
  if (id < 0 || id >= original_sprite_count || w < 1 || h < 1)
    return;
  const Sprite *s = &original_sprites[id];
  int left = x < 0 ? -x : 0, right = x + w > 256 ? 256 - x : w;
  int start = y < top ? top - y : 0, end = y + h > bottom ? bottom - y : h;
  if (left >= right || start >= end)
    return;
  if (w == s->width && h == s->height && !flipx && !flipy && !tint) {
    for (int j = start; j < end; j++) {
      const uint16_t *source = s->pixels + j * s->width + left;
      uint16_t *target = fb + (y + j) * 256 + x + left;
      for (int i = left; i < right; i++) {
        uint16_t color = *source++;
        if (color & 0x8000)
          *target = color;
        target++;
      }
    }
    return;
  }
  unsigned step_x = (unsigned)(s->width << 16) / w,
           step_y = (unsigned)(s->height << 16) / h;
  for (int j = start; j < end; j++) {
    int row = (int)((unsigned)j * step_y >> 16);
    if (row >= s->height)
      row = s->height - 1;
    if (flipy)
      row = s->height - 1 - row;
    const uint16_t *src = s->pixels + row * s->width;
    uint16_t *dest = fb + (y + j) * 256;
    unsigned u = (unsigned)left * step_x;
    for (int i = left; i < right; i++, u += step_x) {
      int col = u >> 16;
      if (col >= s->width)
        col = s->width - 1;
      if (flipx)
        col = s->width - 1 - col;
      uint16_t c = src[col];
      if (c & 0x8000)
        dest[x + i] = tint ? (uint16_t)tint : c;
    }
  }
}
static void world_sprite(int id, float x, float y, float sx, float sy,
                         float camera, int tint) {
  if (id < 0 || id >= original_sprite_count)
    return;
  const Sprite *s = &original_sprites[id];
  float left = fminf(s->x0 * sx, (s->x0 + s->world_w) * sx);
  float high = fmaxf(s->y0 * sy, (s->y0 + s->world_h) * sy);
  int w = (int)(s->world_w * fabsf(sx) * 85.333333f + .5f);
  int h = (int)(s->world_h * fabsf(sy) * 85.333333f + .5f);
  sprite_at(id, (int)(128 + (x + left) * 85.333333f),
            (int)(96 - (y + high - camera) * 85.333333f), w, h, sx < 0, sy < 0,
            tint);
}
static void text(int x, int y, const char *value, uint16_t color, int scale) {
  for (; *value; value++) {
    unsigned ch = (unsigned char)*value;
    if (ch >= 128)
      continue;
    const Glyph *glyph = &original_font[ch];
    if (glyph->sprite >= 0) {
      const Sprite *sprite = &original_sprites[glyph->sprite];
      sprite_at(glyph->sprite, x, y + glyph->offset_y * scale,
                sprite->width * scale, sprite->height * scale, 0, 0, color);
    }
    x += glyph->advance * scale;
  }
}
/* Compact DS interface font. Keep the recovered display font for original art. */
static const unsigned char ui_glyphs[][7] = {
  {14,17,19,21,25,17,14}, /* 0 */
  {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31},
  {30,1,1,14,1,1,30}, {2,6,10,18,31,2,2},
  {31,16,16,30,1,1,30}, {14,16,16,30,17,17,14},
  {31,1,2,4,8,8,8}, {14,17,17,14,17,17,14},
  {14,17,17,15,1,1,14},
  {14,17,17,31,17,17,17}, /* A */
  {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
  {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31},
  {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15},
  {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
  {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17},
  {16,16,16,16,16,16,31}, {17,27,21,21,17,17,17},
  {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
  {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13},
  {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30},
  {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
  {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10},
  {17,17,10,4,10,17,17}, {17,17,10,4,4,4,4},
  {31,1,2,4,8,16,31},
  {0,4,4,0,4,4,0}, /* : */
  {1,2,2,4,8,8,16}, /* / */
  {0,0,0,31,0,0,0}, /* - */
  {0,4,4,31,4,4,0}  /* + */
};
static void ui_text(int x, int y, const char *value, uint16_t color, int scale) {
  for (; *value; value++, x += 6 * scale) {
    int ch = (unsigned char)*value, index = -1;
    if (ch >= '0' && ch <= '9') index = ch - '0';
    else if (ch >= 'A' && ch <= 'Z') index = ch - 'A' + 10;
    else if (ch == ':') index = 36;
    else if (ch == '/') index = 37;
    else if (ch == '-') index = 38;
    else if (ch == '+') index = 39;
    if (index < 0) continue;
    for (int row = 0; row < 7; row++)
      for (int col = 0; col < 5; col++)
        if (ui_glyphs[index][row] & (16 >> col))
          box(x + col * scale, y + row * scale, scale, scale, color);
  }
}
static void ui_center(int y, const char *value, uint16_t color, int scale) {
  int width = ((int)strlen(value) * 6 - 1) * scale;
  ui_text((256 - width) / 2, y, value, color, scale);
}
static void ui_sprite(int id, int x, int y, int w, int h) {
  sprite_at(id, x, y, w, h, 0, 0, 0);
}
void game_render(const Game *g, unsigned short *pixels) {
  fb = pixels;
  top = 10;
  bottom = 181;
  for (int i = 0; i < 256 * 192; i++)
    fb[i] = BLACK;
  if (g->phase == INTRO) {
    int id = g->intro_stage ? art_tettix : art_company;
    const Sprite *s = &original_sprites[id];
    ui_sprite(id, (256 - s->width) / 2, (192 - s->height) / 2, s->width,
              s->height);
    return;
  }
  float camera = (g->phase == TITLE || g->phase == SCORES) ? 0 : g->camera_y;
  for (int i = 0; i < original_scenery_count; i++) {
    const Scenery *s = &original_scenery[i];
    MotionSample m = motion_sample(s->motion, g->frame / 60.f, 1);
    const Motion *motion = &original_motions[s->motion];
    float x = s->x + ((motion->flags & 1) ? m.x : 0),
          y = s->y + ((motion->flags & 1) ? m.y : 0);
    world_sprite(s->sprite, x, y, s->sx * m.sx, s->sy * m.sy, camera, 0);
  }
  if (g->phase == TITLE || g->phase == SCORES) {
    world_sprite(animation_frame(player_clips[0], g->frame), -.68f, -.55f, 1, 1,
                 0, 0);
    if (g->phase == TITLE) {
      ui_sprite(art_logo, 103, 17, 148, 102);
      ui_sprite(art_start, 8, 56, 85, 22);
      ui_sprite(art_scores, 30, 92, 59, 22);
    } else {
      text(70, 32, "YOUR SCORES", WHITE, 2);
      char buf[40];
      snprintf(buf, sizeof buf, "BEST: %d", g->best);
      text(40, 78, buf, LIME, 1);
      snprintf(buf, sizeof buf, "LAST: %d", g->score);
      text(40, 96, buf, PINK, 1);
      text(65, 135, "B: BACK", WHITE, 1);
    }
    return;
  }
  for (int i = 0; i < 6; i++)
    if (g->ramps[i].active) {
      const Ramp *r = &g->ramps[i];
      const RampConfig *c = &original_ramps[i];
      if (r->warning > 0) {
        world_sprite(animation_frame(ramp_alert_clip, g->frame), c->x, -.78f, 1,
                     1, camera, 0);
      } else {
        MotionSample m = motion_sample(c->motion, r->time, 0);
        MotionSample visual = motion_sample(c->visual_motion, r->time, 0);
        world_sprite(animation_frame(c->animation, (int)(r->time * 60)),
                     c->x + m.x, c->y + m.y, visual.sx * (i < 3 ? -1 : 1),
                     visual.sy, camera, 0);
      }
    }
  for (int i = 0; i < MAX_ENEMIES; i++) {
    const Enemy *e = &g->enemies[i];
    if (!e->active)
      continue;
    if (e->stage == 3) {
      for (int p = 0; p < 10; p++) {
        int x = (int)(128 + (e->x + (p % 5 - 2) * e->life) * 85.333333f);
        int y = (int)(96 - (e->y + (p / 5 ? 1 : -1) * e->life - camera) *
                               85.333333f);
        box(x, y, 2, 2, p % 2 ? PINK : LIME);
      }
    } else {
      world_sprite(game_enemy_sprite(e), e->x, e->y, e->sx, e->sy, camera,
                   e->hurt ? (e->hurt % 2 ? PINK : LIME) : 0);
      if (e->attack_age >= 0 && original_enemies[e->config].kind == 2) {
        world_sprite(animation_frame(reticle_clip, e->attack_age), e->target_x,
                     -.8f, 1, 1, camera, 0);
        if (e->attack_state == 2)
          line((int)(128 + e->x * 85.333333f),
               (int)(96 - (e->y - camera) * 85.333333f),
               (int)(128 + e->target_x * 85.333333f),
               (int)(96 - (-.8f - camera) * 85.333333f), PINK);
      }
      if (e->attack_age >= 0 && original_enemies[e->config].kind == 4 &&
          e->attack_state == 2) {
        int x = (int)(128 + e->x * 85.333333f);
        box(x - 3, (int)(96 - (e->y - camera) * 85.333333f), 6,
            (int)((e->y + .8f) * 85.333333f), LIME);
      }
    }
  }
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    const Projectile *p = &g->projectiles[i];
    if (!p->active)
      continue;
    if (p->kind == 0)
      world_sprite(animation_frame(bomb_clip, p->age), p->x, p->y, 1, 1, camera,
                   0);
    else if (p->kind == 1) {
      int x = (int)(128 + p->x * 85.333333f),
          y = (int)(96 - (p->y - camera) * 85.333333f);
      for (int a = -2; a <= 2; a++)
        line(x, y, x + a * 7, y - 10 + (a & 1) * 5, PINK);
    } else
      world_sprite(animation_frame(ghost_clip, p->age), p->x, p->y, 1, 1,
                   camera, 0);
  }
  if (g->phase != DEAD && g->phase != FINISHED)
    world_sprite(animation_frame(g->clip, g->clip_tick), g->x, g->y, 1, 1,
                 camera, 0);
  if (g->power) {
    if (g->shield)
      world_sprite(animation_frame(shield_clip, g->frame), g->x, g->y, 1, 1,
                   camera, 0);
    else if (!g->power_time)
      world_sprite(animation_frame(ghost_clip, g->frame), g->ghost_x, -.65f, 1,
                   1, camera, 0);
  }
  top = 0;
  bottom = 192;
  char buf[32];
  snprintf(buf, sizeof buf, "SCORE: %d", g->score);
  ui_text(2, 2, buf, WHITE, 1);
  if (g->combo > 1) {
    snprintf(buf, sizeof buf, "AIR COMBO X%d", g->combo);
    text(140, 14, buf, PINK, 1);
  }
  box(152, 2, 78, 5, WHITE);
  box(153, 3, (g->level * 5 + g->progress) * 76 / 20, 3, PINK);
  if (g->phase == PAUSED) {
    box(60, 69, 136, 43, BLACK);
    text(74, 77, "PAUSED", PINK, 2);
    text(79, 99, "START: RESUME", WHITE, 1);
  }
  if (g->phase == DEAD) {
    box(12, 80, 232, 33, BLACK);
    ui_sprite(art_dead, 23, 84, 210, 22);
  } else if (g->phase == FINISHED)
    ui_sprite(art_win, 23, 55, 210, 65);
  if (g->nuke_flash && (g->nuke_flash & 1))
    for (int i = 0; i < 256 * 192; i++)
      fb[i] = WHITE;
}
void game_render_bottom(const Game *g, unsigned short *pixels) {
  fb = pixels;
  top = 0;
  bottom = 192;
  for (int i = 0; i < 256 * 192; i++)
    fb[i] = BLACK;
  ui_center(8, "COOL PIZZA", PINK, 2);

  if (g->phase == INTRO) {
    ui_center(124, "A / TAP: NEXT", WHITE, 1);
    return;
  }
  if (g->phase == TITLE) {
    ui_center(59, "START / TAP TO PLAY", WHITE, 1);
    ui_center(78, "X: SCORES", LIME, 1);
    ui_center(112, "LEFT / RIGHT: STEER", WHITE, 1);
    ui_center(128, "A / TAP: ATTACK", WHITE, 1);
    ui_center(144, "Y: GHOST POWER", LIME, 1);
    ui_center(174, "SECRET LIBRARY / TETTIX", WHITE, 1);
    return;
  }
  char buf[40];
  ui_center(40, g->phase == SCORES ? "BEST SCORE" : "SCORE", WHITE, 1);
  snprintf(buf, sizeof buf, "%d", g->phase == SCORES ? g->best : g->score);
  ui_center(54, buf, WHITE, 2);
  if (g->phase == SCORES) {
    snprintf(buf, sizeof buf, "LAST: %d", g->score);
    ui_center(94, buf, LIME, 1);
    ui_center(148, "B: BACK", WHITE, 1);
    return;
  }
  snprintf(buf, sizeof buf, "WAVE %d / %d", g->wave, g->wave_count);
  ui_center(82, buf, LIME, 1);
  snprintf(buf, sizeof buf, "BEST: %d", g->best);
  ui_center(96, buf, WHITE, 1);
  if (g->phase == DEAD || g->phase == FINISHED) {
    ui_center(126, "A / TAP: AGAIN", PINK, 1);
    ui_center(148, "B: MENU", WHITE, 1);
  } else {
    if (g->phase == PAUSED) {
      ui_center(120, "PAUSED", PINK, 2);
      ui_center(145, "START: RESUME", WHITE, 1);
    } else if (g->power && !g->power_time) {
      /* Keep the entire button inside the touch target at y=110..155. */
      static const char *const power_names[] = {"JUMP", "SHOT", "NUKE", "SHIELD"};
      box(35, 110, 186, 46, WHITE);
      box(37, 112, 182, 42, BLACK);
      ui_center(117, "GHOST POWER", WHITE, 1);
      snprintf(buf, sizeof buf, "Y / TAP: %s", power_names[g->power - 1]);
      ui_center(132, buf, LIME, 2);
    } else {
      ui_center(120, "KEEP YOUR COMBO IN THE AIR", PINK, 1);
      ui_center(140, "A / TAP TO STRIKE", WHITE, 1);
    }
    ui_center(174, "START: PAUSE   B: MENU", WHITE, 1);
  }
}
