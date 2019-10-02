// include SPI, MP3 and SD libraries
#include "filemanager.h"

char curDirPath[64] = "/";
char curFilePath[64];
char fileList[16][16];
char fileNameList[256];
mp3header header;

/// File listing helper
int getFileList(File dir) {
  int count = 0;
  int str_p = 0;

  while(true) {
     
    File entry = dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    strcpy(fileList[count++], entry.name());
    strcpy(&fileNameList[str_p], entry.name());
    str_p += strlen(entry.name());
    fileNameList[str_p++] = '\n';
    entry.close();
  }
  if (count != 0 && fileNameList[str_p-1] == '\n')
    fileNameList[str_p-1] == '\0';
  return count;
}

/// File listing helper
void printDirectory(File dir, int numTabs) {
  while(true) {
     
    File entry =  dir.openNextFile();
    if (! entry) {
      // no more files
      break;
    }
    for (uint8_t i=0; i<numTabs; i++) {
      Serial.print('\t');
    }
    Serial.print(entry.name());
    if (entry.isDirectory()) {
      Serial.println("/");
      printDirectory(entry, numTabs+1);
    } else {
      // files have sizes, directories do not
      Serial.print("\t\t");
      Serial.println(entry.size(), DEC);
    }
    entry.close();
  }
}

int header_bitrate(mp3header *h)
{
  int bitrate[2][3][15] = { 
  { /* MPEG 2.0 */
    {0,32,48,56,64,80,96,112,128,144,160,176,192,224,256},  /* layer 1 */
    {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160},       /* layer 2 */
    {0,8,16,24,32,40,48,56,64,80,96,112,128,144,160}        /* layer 3 */

  },

  { /* MPEG 1.0 */
    {0,32,64,96,128,160,192,224,256,288,320,352,384,416,448}, /* layer 1 */
    {0,32,48,56,64,80,96,112,128,160,192,224,256,320,384},    /* layer 2 */
    {0,32,40,48,56,64,80,96,112,128,160,192,224,256,320}      /* layer 3 */
  }
};
  return bitrate[h->version & 1][3-h->layer][h->bitrate];
}

int header_frequency(mp3header *h) {
  int frequencies[3][4] = {
   {22050,24000,16000,50000},  /* MPEG 2.0 */
   {44100,48000,32000,50000},  /* MPEG 1.0 */
   {11025,12000,8000,50000}    /* MPEG 2.5 */
  };
  return frequencies[h->version][h->freq];
}

int sameConstant(mp3header *h1, mp3header *h2)
{
  if ((*(uint*)h1) == (*(uint*)h2)) return 1;

  if ((h1->version       == h2->version         ) &&
      (h1->layer         == h2->layer           ) &&
      (h1->crc           == h2->crc             ) &&
      (h1->freq          == h2->freq            ) &&
      (h1->mode          == h2->mode            ) &&
      (h1->copyright     == h2->copyright       ) &&
      (h1->original      == h2->original        ) &&
      (h1->emphasis      == h2->emphasis        )) 
    return 1;
  else return 0;
}

int frame_length(mp3header *header)
{
  int frame_size_index[3] = {24000, 72000, 72000};
  return header->sync == 0xFFE ? 
        (frame_size_index[3-header->layer]*((header->version&1)+1)*
        header_bitrate(header)/header_frequency(header))+
        header->padding : 1;
}

/* Get next MP3 frame header.
   Return codes:
   positive value = Frame Length of this header
   0 = No, we did not retrieve a valid frame header
*/

int get_header(File file, mp3header *header)
{
  char buffer[FRAME_HEADER_SIZE+1];
  uint32_t bytes;
  int fl;

  bytes = file.read(buffer, FRAME_HEADER_SIZE);
  if (bytes<1) {
    header->sync=0;
    return 0;
  }
  header->sync=(((int)buffer[0]<<4) | ((int)(buffer[1]&0xE0)>>4));
  if (buffer[1] & 0x10) header->version=(buffer[1] >> 3) & 1;
  else header->version=2;
  header->layer=(buffer[1] >> 1) & 3;
  header->bitrate=(buffer[2] >> 4) & 0x0F;
  if ((header->sync != 0xFFE) || (header->layer != 1) || (header->bitrate == 0xF)) {
    header->sync=0;
    return 0;
  }
  header->crc=buffer[1] & 1;
  header->freq=(buffer[2] >> 2) & 0x3;
  header->padding=(buffer[2] >>1) & 0x1;
  header->extension=(buffer[2]) & 0x1;
  header->mode=(buffer[3] >> 6) & 0x3;
  header->mode_extension=(buffer[3] >> 4) & 0x3;
  header->copyright=(buffer[3] >> 3) & 0x1;
  header->original=(buffer[3] >> 2) & 0x1;
  header->emphasis=(buffer[3]) & 0x3;

  /* Final sanity checks: bitrate 1111b and frequency 11b are reserved (invalid) */
  if (header->bitrate == 0x0F || header->freq == 0x3) {
    return 0;
  }

  return ((fl=frame_length(header)) >= MIN_FRAME_SIZE ? fl : 0); 
}

int get_first_header(File mp3, long startpos, mp3header *header)
{
  int k, l=0;
  unsigned char c;
  mp3header h, h2;
  long valid_start=0;
  uint32_t bytes;
  
  mp3.seek(startpos);
  while (1) {
    //while ((c=fgetc(mp3->file)) != 255 && (c != EOF));
    do {
      c = mp3.read();
      if (c == 255 || c == EOF) break;
    } while (1);

    if (c == 255) {
      mp3.seek(mp3.position()-1);
      valid_start=mp3.position();
      if ((l=get_header(mp3,&h))) {
        mp3.seek(l-FRAME_HEADER_SIZE+mp3.position());
        for (k=1; (k < MIN_CONSEC_GOOD_FRAMES) && (mp3.size()-mp3.position() >= FRAME_HEADER_SIZE); k++) {
          if (!(l=get_header(mp3,&h2))) break;
          if (!sameConstant(&h,&h2)) break;
          mp3.seek(l-FRAME_HEADER_SIZE+mp3.position());
        }
        if (k == MIN_CONSEC_GOOD_FRAMES) {
          mp3.seek(valid_start);
          memcpy(header,&h2,sizeof(mp3header));
          return 1;
        } 
      }
    } else {
      return 0;
    }
  }
  return 0;  
}

int getMP3Info(File mp3)
{
  int frames, seconds;
  int vbr=-1;
  int bitrate,lastrate;
  int counter=0;
  int sample_pos,data_start=0;

  if (get_first_header(mp3,0L,&header)) {
    data_start=mp3.position();
    lastrate=15-header.bitrate;
    while ((counter < NUM_SAMPLES) && lastrate) {
      sample_pos=(counter*(mp3.size()/NUM_SAMPLES+1))+data_start;
      if (get_first_header(mp3,sample_pos,&header)) {
        bitrate=15-header.bitrate;
      } else {
        bitrate=-1;
      }

      if (bitrate != lastrate) {
        vbr=1;
      }
      lastrate=bitrate;
      counter++;
    }
    frames=(mp3.size()-data_start)/frame_length(&header);
    seconds = (int)((float)(frame_length(&header)*frames)/
               (float)(header_bitrate(&header)*125)+0.5);
    //float vbr_average = (float)header_bitrate(header);
    return seconds;
  }
  return 0;
}

int getID3(File mp3, char *title, char *artist, char *album)
{
    char fbuf[4];
    mp3.seek(mp3.size()-128);
    int bytes = mp3.read(fbuf, 3); fbuf[3] = '\0';

    if (!strcmp((const char *)"TAG", (const char *)fbuf)) {
      mp3.read(title, 30); title[30] = '\0';
      mp3.read(artist, 30); artist[30] = '\0';
      mp3.read(album, 30); album[30] = '\0';
      //mp3.read(&(mp3->id3.year), 4, &bytes); mp3->id3.year[4] = '\0';
      //mp3.read(&(mp3->id3.comment), 30, &bytes); mp3->id3.comment[30] = '\0';
      return 1;
   }
   return 0;
}

int unicode_to_utf8(unsigned short uc, char* UTF8)
{
    if (uc <= 0x7f) {
        UTF8[0] = (char) uc;
        return 1;
    }
    else if (uc <= 0x7ff) { 
        UTF8[0] = (char) 0xc0 | ((uc >> 6)&0x1f);
        UTF8[1] = (char) 0x80 | (uc & 0x3f);
        UTF8[2] = (char) 0;
        return 2;
    }
    else if (uc <= 0xffff) {
        UTF8[0] = (char) 0xe0 | ((uc >> 12) & 0x0f);
        UTF8[1] = (char) 0x80 | ((uc >> 6) & 0x3f);
        UTF8[2] = (char) 0x80 | (uc & 0x3f);
        return 3;
    }
    return 0;
}

void ucs_to_utf8(char *ucs, char* UTF8)
{
  int i, j;
  unsigned short unicode;
  for(i=0, j=0;ucs[i];i++) {
    if (ucs[i] < 0x80) {
      unicode = ucs[i];
    } else {
      unicode = ff_convert((unsigned short)((ucs[i]<<8) | ucs[i+1]), 1);
      i++;
    }
    int count = unicode_to_utf8(unicode, &UTF8[j]);
    j += count;
  }
  UTF8[j] = '\0';
}
