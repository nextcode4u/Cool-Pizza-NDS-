#include "audio.h"
#include "game.h"
#include "save.h"
#include <nds.h>
static Game game;
static volatile unsigned vblank_ticks;
static unsigned rendered_frames;
static void on_vblank(void) { vblank_ticks++; }
static unsigned short top_pixels[256 * 192] __attribute__((aligned(32)));
static unsigned short bottom_pixels[256 * 192] __attribute__((aligned(32)));
int main(void) {
  videoSetMode(MODE_5_2D);
  videoSetModeSub(MODE_5_2D);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_BG);
  vramSetBankC(VRAM_C_SUB_BG);
  lcdMainOnTop();
  int bg = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
  int sub = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
  unsigned short *base = bgGetGfxPtr(bg);
  int visible = 0;
  game_init(&game);
  audio_init();
  save_init(&game);
  irqSet(IRQ_VBLANK, on_vblank);
  irqEnable(IRQ_VBLANK);
  unsigned last_tick = vblank_ticks;
  Phase previous = (Phase)-1;
  int tick = 0;
  int saved_best = game.best, saved_plays = game.plays;
  while (pmMainLoop()) {
    scanKeys();
    unsigned down = keysDown(), held = keysHeld();
    touchPosition touch;
    touchRead(&touch);
    int tapped = !!(down & KEY_TOUCH), power = tapped && touch.py >= 110 &&
                                               touch.py < 156 && game.power &&
                                               game.phase == PLAYING;
    Input input = {.steer = !!(held & KEY_RIGHT) - !!(held & KEY_LEFT),
                   .strike = !!(down & KEY_A) || (tapped && !power),
                   .start = !!(down & KEY_START),
                   .power = !!(down & KEY_Y) || power,
                   .back = !!(down & KEY_B),
                   .scores = !!(down & KEY_X),
                   .mute = !!(down & KEY_SELECT)};
    /* Rendering can take multiple refreshes. Keep the original 60 Hz physics,
       motion and animation clocks independent of presentation throughput. */
    unsigned now = vblank_ticks, elapsed = now - last_tick;
    if (!elapsed) {
      swiWaitForVBlank();
      now = vblank_ticks;
      elapsed = now - last_tick;
    }
    last_tick = now;
    if (elapsed > 8)
      elapsed = 8;
    unsigned sounds = 0;
    for (unsigned step = 0; step < elapsed; step++) {
      game_step(&game, input);
      sounds |= game.sounds;
      input = (Input){.steer = input.steer};
    }
    game.sounds = sounds;
    if (game.best != saved_best || game.plays != saved_plays) {
      save_stats(&game);
      saved_best = game.best;
      saved_plays = game.plays;
    }
    /* Keep the live state observable by DS debuggers, which read uncached RAM.
     */
    DC_FlushRange(&game, sizeof game);
    audio_update(&game);
    rendered_frames++;
    DC_FlushRange(&rendered_frames, sizeof rendered_frames);
    game_render(&game, top_pixels);
    int update = previous != game.phase || ++tick >= 8;
    if (update) {
      game_render_bottom(&game, bottom_pixels);
      previous = game.phase;
      tick = 0;
    }
    swiWaitForVBlank();
    DC_FlushRange(top_pixels, sizeof top_pixels);
    dmaCopy(top_pixels, base + (visible ? 0 : 256 * 256), sizeof top_pixels);
    visible = !visible;
    bgSetMapBase(bg, visible ? 8 : 0);
    if (update) {
      DC_FlushRange(bottom_pixels, sizeof bottom_pixels);
      dmaCopy(bottom_pixels, bgGetGfxPtr(sub), sizeof bottom_pixels);
    }
    audio_update(&game);
  }
  audio_shutdown();
  return 0;
}
