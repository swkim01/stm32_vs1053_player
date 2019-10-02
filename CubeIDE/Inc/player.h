#ifndef __PLAYER_H
#define __PLAYER_H

#include "filemanager.h"
#include "vs10xx.h"

#define TRUE  1
#define FALSE 0

/** Playing states definations. */
volatile typedef enum {
  PS_STOP,         // Player stop
  PS_PLAY,         // Start to player
  PS_PAUSE,        // Pause play
  PS_RECORDING,    // Recording states
} playerStatetype;

extern playerStatetype  playerState;
extern unsigned short flacPlugin[5433];
extern unsigned short recPlugin[40];

void player_init(void);
void player_play(char *file);
void player_feedbuffer(void);
void player_stop(void);
void player_pause(int pause);
void player_sinetest(uint8_t n, uint16_t ms);

#endif
