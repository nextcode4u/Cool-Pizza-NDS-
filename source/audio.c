#include "audio.h"
#include <filesystem.h>
#include <maxmod9.h>
#include <nds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  short *pcm;
  size_t count;
  unsigned used;
} Sample;
typedef struct {
  int sample;
  size_t position;
} Voice;
static Sample samples[25];
static Voice voices[9];
static FILE *music;
static int enabled, paused, muted, loop_music, death_music;
static Phase previous = (Phase)-1;
static unsigned stamp;
static int sound_frame = -1;
static size_t cached_bytes;

static void open_music(const char *name, int loop, int death) {
  if (music)
    fclose(music);
  char path[80];
  snprintf(path, sizeof(path), "nitro:/audio/%s.pcm", name);
  music = fopen(path, "rb");
  loop_music = loop;
  death_music = death;
}
static int in_use(int sample) {
  for (int i = 0; i < 9; i++)
    if (voices[i].sample == sample)
      return 1;
  return 0;
}
static void play_effect(int id) {
  int index = id - 126;
  if (index < 0 || index >= 25)
    return;
  Sample *s = &samples[index];
  if (id == 126)
    for (int v = 0; v < 8; v++)
      if (voices[v].sample == 134 - 126)
        voices[v].sample = -1;
  if (!s->pcm) {
    char path[80];
    snprintf(path, sizeof(path), "nitro:/audio/sfx%d.pcm", id);
    FILE *f = fopen(path, "rb");
    if (!f)
      return;
    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    rewind(f);
    if (bytes <= 0 || bytes > 400000) {
      fclose(f);
      return;
    }
    while (cached_bytes + (size_t)bytes > 950000) {
      int oldest = -1;
      for (int j = 0; j < 25; j++)
        if (samples[j].pcm && !in_use(j) &&
            (oldest < 0 || samples[j].used < samples[oldest].used))
          oldest = j;
      if (oldest < 0) {
        voices[0].sample = -1;
        break;
      }
      cached_bytes -= samples[oldest].count * 2;
      free(samples[oldest].pcm);
      samples[oldest].pcm = NULL;
    }
    s->pcm = malloc((size_t)bytes);
    if (!s->pcm) {
      fclose(f);
      return;
    }
    s->count = fread(s->pcm, 2, (size_t)bytes / 2, f);
    fclose(f);
    cached_bytes += s->count * 2;
  }
  s->used = ++stamp;
  int slot = 0;
  for (int i = 0; i < 8; i++)
    if (voices[i].sample < 0) {
      slot = i;
      break;
    }
  voices[slot] = (Voice){index, 0};
}
static mm_word fill_stream(mm_word length, mm_addr destination,
                           mm_stream_formats format) {
  (void)format;
  short *dest = destination;
  memset(dest, 0, length * 4);
  if (paused || muted)
    return length;
  size_t got = 0;
  while (music && got < length) {
    size_t read = fread(dest + got * 2, 4, length - got, music);
    got += read;
    if (got == length)
      break;
    if (death_music) {
      open_music("scores", 1, 0);
      continue;
    }
    if (loop_music) {
      rewind(music);
      continue;
    }
    fclose(music);
    music = NULL;
  }
  for (unsigned i = 0; i < length; i++) {
    int left = dest[i * 2] * 3 / 5, right = dest[i * 2 + 1] * 3 / 5;
    for (int v = 0; v < 9; v++) {
      Voice *voice = &voices[v];
      if (voice->sample < 0)
        continue;
      const Sample *s = &samples[voice->sample];
      if (voice->position >= s->count) {
        if (v == 8)
          voice->position = 0;
        else {
          voice->sample = -1;
          continue;
        }
      }
      int sample = s->pcm[voice->position++] / 2;
      left += sample;
      right += sample;
    }
    dest[i * 2] = (short)(left < -32768 ? -32768 : left > 32767 ? 32767 : left);
    dest[i * 2 + 1] = (short)(right < -32768  ? -32768
                              : right > 32767 ? 32767
                                              : right);
  }
  return length;
}
int audio_init(void) {
  for (int i = 0; i < 9; i++)
    voices[i].sample = -1;
  enabled = nitroFSInit(NULL);
  if (!enabled)
    return 0;
  mm_ds_system system = {0};
  mmInit(&system);
  open_music("title", 1, 0);
  mm_stream stream = {0};
  stream.sampling_rate = 22050;
  stream.buffer_length = 4096;
  stream.callback = fill_stream;
  stream.format = MM_STREAM_16BIT_STEREO;
  stream.timer = MM_TIMER0;
  stream.manual = true;
  mmStreamOpen(&stream);
  previous = TITLE;
  return 1;
}
void audio_update(const Game *g) {
  if (!enabled)
    return;
  paused = g->phase == PAUSED;
  muted = g->muted;
  if (g->phase != previous) {
    if (g->phase == TITLE && previous != INTRO)
      open_music("title", 1, 0);
    if (g->phase == SCORES || g->phase == FINISHED)
      open_music("scores", 1, 0);
    if (g->phase == PLAYING && previous != PAUSED)
      open_music("level", 1, 0);
    if (g->phase == DEAD)
      open_music("death", 0, 1);
    previous = g->phase;
  }
  if (sound_frame != g->frame) {
    sound_frame = g->frame;
    for (int i = 0; i < 25; i++)
      if (g->sounds & (1u << i))
        play_effect(126 + i);
  }
  int flies = 0;
  if (g->phase == PLAYING || g->phase == PAUSED)
    for (int i = 0; i < MAX_ENEMIES; i++)
      if (g->enemies[i].active && g->enemies[i].stage != 3 &&
          original_enemies[g->enemies[i].config].kind == 0)
        flies++;
  if (flies && voices[8].sample < 0) {
    play_effect(135);
    for (int i = 0; i < 8; i++)
      if (voices[i].sample == 9) {
        voices[8] = voices[i];
        voices[i].sample = -1;
        break;
      }
  }
  if (!flies)
    voices[8].sample = -1;
  mmStreamUpdate();
}
void audio_shutdown(void) {
  if (enabled)
    mmStreamClose();
  if (music)
    fclose(music);
  for (int i = 0; i < 25; i++)
    free(samples[i].pcm);
}
