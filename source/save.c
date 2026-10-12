#include "save.h"
#include <fat.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
static int available;
typedef struct {
  uint32_t magic, best, plays, check;
} Stats;
int save_init(Game *g) {
  available = access("fat:/", F_OK) == 0 || fatInitDefault();
  if (!available)
    return 0;
  FILE *f = fopen("fat:/coolpizza.stats", "rb");
  if (!f)
    return 1;
  Stats s;
  int valid = fread(&s, sizeof s, 1, f) == 1;
  fclose(f);
  if (valid && s.magic == 0x34315043u &&
      s.check == (s.magic ^ s.best ^ s.plays ^ 0x51da9267u) &&
      s.best <= 0x7fffffffu && s.plays <= 1000000000u) {
    g->best = (int)s.best;
    g->plays = (int)s.plays;
  }
  return 1;
}
void save_stats(const Game *g) {
  if (!available)
    return;
  Stats s = {0x34315043u, (uint32_t)g->best, (uint32_t)g->plays, 0};
  s.check = s.magic ^ s.best ^ s.plays ^ 0x51da9267u;
  FILE *f = fopen("fat:/coolpizza.stats.tmp", "wb");
  if (!f)
    return;
  int valid = fwrite(&s, sizeof s, 1, f) == 1;
  valid = fclose(f) == 0 && valid;
  if (valid)
    rename("fat:/coolpizza.stats.tmp", "fat:/coolpizza.stats");
}
