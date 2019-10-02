// Specifically for use with the Adafruit Feather, the pins are pre-set here!
#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <SD.h>
#include <Adafruit_VS1053.h>
#include "filemanager.h"

U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, /* clock=*/ 5, /* data=*/ 4, /* reset=*/ U8X8_PIN_NONE);

// These are the pins used
#define VS1053_RESET   8     // VS1053 reset pin (not used!)

// Feather ESP8266
#if defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_NRF52832_FEATHER )
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin

// Feather M4, M0, 328, nRF52840 or 32u4
#else
/*
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin
*/
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS      7     // VS1053 Data/command select pin (output)
  #define CARDCS          9     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     2     // VS1053 Data request, ideally an Interrupt pin

#endif

Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(VS1053_RESET, VS1053_CS, VS1053_DCS, VS1053_DREQ, CARDCS);

int volume = 50;

#define BUTTON_PLAY     A0
#define BUTTON_DOWN     A1
#define BUTTON_UP       A2
#define BUTTON_STOP     A3

typedef enum {
  FILE_MODE,
  PLAY_MODE
} run_state;

File curDir;
uint8_t curSel = 1;
char title[31], album[31], artist[31];
char title8[40], album8[40], artist8[40];
int tlen, txpos=128;
int sec, seconds;
unsigned long timeVal;
char timebuf[10]="00:00";
run_state rstate = FILE_MODE;
int pstate = 0; // 1: pause

void setup() {
  Serial.begin(115200);

  // Wait for serial port to be opened, remove this line for 'standalone' operation
  while (!Serial) { delay(1); }
  delay(500);
  Serial.println("\n\nAdafruit VS1053 Feather Test");

  /* Setup U8G2 for SSD1306 I2C Module */
  u8g2.begin(/*Select=*/BUTTON_PLAY, /*Right/Next=*/BUTTON_DOWN, /*Left/prev=*/BUTTON_UP,/*Up=*/-1,/*Down=*/-1,/*Home/Cancel*/BUTTON_STOP);
  u8g2.enableUTF8Print();    // enable UTF8 support for the Arduino print() function
  u8g2.setFont(u8g2_font_unifont_t_korean2);  
  u8g2.setFontDirection(0);
  
  /* Initialize  VS1053 MP3 Module */
  if (! musicPlayer.begin()) {
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }

  Serial.println(F("VS1053 found"));
 
  musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working
  
  if (!SD.begin(CARDCS)) {
    Serial.println(F("SD failed, or not present"));
    while (1);  // don't do anything more
  }
  Serial.println("SD OK!");
  
  /* List files */
  curDir = SD.open(curDirPath);
  printDirectory(curDir, 0);
  curDir.rewindDirectory();
  
  /* Set volume for left, right channels. lower numbers == louder volume! */
  musicPlayer.setVolume(volume,volume);
  
#if defined(__AVR_ATmega32U4__) 
  // Timer interrupts are not suggested, better to use DREQ interrupt!
  // but we don't have them on the 32u4 feather...
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT); // timer int
#else
  // If DREQ is on an interrupt pin we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int
#endif

}

void loop() {

  if (rstate == FILE_MODE) {
    /* When File Mode, display file list on OLED */
    int filecount = getFileList(curDir);
    curSel = u8g2.userInterfaceSelectionList(curDirPath, curSel, fileNameList);
    if (curSel == 0) { // Move to Prev/Up directory
      int i = String(curDirPath).lastIndexOf("/");
      curDirPath[i?i:i+1] = '\0'; 
      curDir = SD.open(curDirPath);
    }
    else {
      /* Select a File */
      strcpy(curFilePath,curDirPath);
      if (strcmp(curFilePath, "/"))
        strcat(curFilePath,"/");
      strcat(curFilePath,fileList[curSel-1]);
      File curFile = SD.open(curFilePath);
      if (curFile.isDirectory()) { // Go to the subdirectory
        strcpy(curDirPath, curFilePath);
        curDir = curFile;
        curSel = 1;
      } else {
        // Select the mp3 file and change to play mode
        String filename = String(curFile.name());
        if (filename.endsWith(".mp3") || filename.endsWith(".MP3")) {
          getID3(curFile, title, artist, album);
          ucs_to_utf8(title, title8);
          ucs_to_utf8(artist, artist8);
          ucs_to_utf8(album, album8);
          tlen = strlen(title8);
          seconds = getMP3Info(curFile);
          Serial.print(title8); Serial.print(" ");
          Serial.print(artist8); Serial.print(" ");
          Serial.println(album8);
          Serial.println(seconds); Serial.println("...");
          curFile.close();
          musicPlayer.playFullFile(curFilePath);
          rstate = PLAY_MODE;
          sec = pstate = 0;
          return;
        }
      }
    }
    delay(100);
  }
  
  else {
    /* When Play Mode, The File is playing in the background */
    if (millis() - timeVal >= 1000) {
      sec++;
      sprintf(timebuf,"%02d:%02d", sec/60, sec%60);
      timeVal = millis();
    }
    if (pstate)
      timeVal = millis();

    /* Display artist, album, song title on OLED */
    txpos = (txpos+tlen*8>0)?txpos-5:128;
    u8g2.firstPage();
    do {
      u8g2.drawUTF8(0, 15, "Playing...");
      u8g2.drawUTF8(0, 30, artist8);
      u8g2.drawUTF8(60, 30, album8);
      u8g2.drawUTF8(txpos, 45, title8);
      u8g2.drawRFrame(0,50,80,10,1);
      u8g2.drawBox(0,50,(80.0*sec/seconds),10);
      u8g2.drawUTF8(84, 60, timebuf);
    } while ( u8g2.nextPage() );

    if (musicPlayer.stopped()) {
      Serial.println("Done playing music");
      delay(10);  // we're done! do nothing...
      rstate = FILE_MODE;
    }

    /* if we get BUTTON_STOP, stop! */
    if (!digitalRead(BUTTON_STOP)) {
      musicPlayer.stopPlaying();
      rstate = FILE_MODE;
    }
    
    /* if we get BUTTON_PLAY, pause/unpause! */
    if (!digitalRead(BUTTON_PLAY)) {
      if (! musicPlayer.paused()) {
        Serial.println("Paused");
        musicPlayer.pausePlaying(true);
        pstate = 1;
      } else {
        Serial.println("Resumed");
        musicPlayer.pausePlaying(false);
        pstate = 0;
      }
    }
    
    /* if we get BUTTON_UP, volume up! */
    if (!digitalRead(BUTTON_UP)) {
      // Set volume for left, right channels. lower numbers == louder volume!
      volume -= 5;
      if (volume < 0) volume = 0;
      musicPlayer.setVolume(volume,volume);
    }

    /* if we get BUTTON_DOWN, volume down! */
    if (!digitalRead(BUTTON_DOWN)) {
      // Set volume for left, right channels. lower numbers == louder volume!
      volume += 5;
      if (volume > 100) volume = 100;
      musicPlayer.setVolume(volume,volume);
    }
    delay(10);
  }
}
