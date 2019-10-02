#ifndef __FILEMANAGER_H
#define __FILEMANAGER_H

#include "main.h"
#include "fatfs.h"
#include "ff_gen_drv.h"

/* MIN_CONSEC_GOOD_FRAMES defines how many consecutive valid MP3 frames
   we need to see before we decide we are looking at a real MP3 file */
#define MIN_CONSEC_GOOD_FRAMES 4
#define FRAME_HEADER_SIZE 4
#define MIN_FRAME_SIZE 21
#define NUM_SAMPLES 4

typedef struct {
        uint32_t        sync;
        uint32_t        version;
        uint32_t        layer;
        uint32_t        crc;
        uint32_t        bitrate;
        uint32_t        freq;
        uint32_t        padding;
        uint32_t        extension;
        uint32_t        mode;
        uint32_t        mode_extension;
        uint32_t        copyright;
        uint32_t        original;
        uint32_t        emphasis;
} mp3header;

extern char curDirPath[64];
extern char curFilePath[64];
extern char fileList[16][16];
extern char fileNameList[256];

int getFileList(DIR *dir);
void printDirectory(DIR *dir, int numTabs);
int getMP3Info(FIL *mp3);
int getID3(FIL *mp3, char *title, char *artist, char *album);
void ucs_to_utf8(char *ucs, char* UTF8);
unsigned short ff_convert (unsigned short chr, unsigned int dir);

#endif
