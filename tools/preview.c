#include "game.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  Game g;
  game_init(&g);
  game_start(&g, 17);
  if (argc > 1)
    game_enter_wave(&g, atoi(argv[1]));
  int ticks = argc > 2 ? atoi(argv[2]) : 120;
  for (int i = 0; i < ticks; i++)
    game_step(&g, (Input){.strike = i % 8 == 0});
  unsigned short fb[256 * 384];
  game_render(&g, fb);
  game_render_bottom(&g, fb + 256 * 192);
  FILE *f = fopen("build/preview.ppm", "wb");
  if (!f)
    return 1;
  fprintf(f, "P6\n256 384\n255\n");
  for (int i = 0; i < 256 * 384; i++) {
    unsigned p = fb[i];
    unsigned char rgb[3] = {(p & 31) * 255 / 31, ((p >> 5) & 31) * 255 / 31,
                            ((p >> 10) & 31) * 255 / 31};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  return 0;
}
