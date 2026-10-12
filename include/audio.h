#ifndef COOLPIZZA_AUDIO_H
#define COOLPIZZA_AUDIO_H
#include "game.h"
int audio_init(void);
void audio_update(const Game *game);
void audio_shutdown(void);
#endif
