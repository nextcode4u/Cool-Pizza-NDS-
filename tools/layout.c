#include "game.h"
#include <stddef.h>
#include <stdio.h>
#define OFFSET(member) printf("\"%s\":%zu,",#member,offsetof(Game,member))
int main(void) {
 putchar('{');
 OFFSET(phase);OFFSET(frame);OFFSET(wave);OFFSET(clock);OFFSET(score);OFFSET(combo);
 OFFSET(attack);OFFSET(clip);OFFSET(clip_tick);OFFSET(sounds);OFFSET(x);OFFSET(y);OFFSET(vy);
 OFFSET(wave_count);OFFSET(wave_id);OFFSET(enemies);OFFSET(projectiles);OFFSET(ramps);
 printf("\"size\":%zu,\"enemy_size\":%zu}",sizeof(Game),sizeof(Enemy));
}
