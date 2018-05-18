#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <./Fonts/FreeSerifBold24pt7b.h>
#include <gfxfont.h>
#include <avr/pgmspace.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

#define XIAOMI_PORT Serial

volatile unsigned char _newDataFlag = 0; //assign '1' for renew display once

const unsigned char _commandsWeWillSend[] = {1, 8, 10}; //insert INDEXES of commands, wich will be send in a circle

        // INDEX                     //0     1     2     3    etc.
const unsigned char _q[] PROGMEM = {0x3B, 0x31, 0x20, 0x1B, 0x10, 0x1A, 0x69, 0x3E, 0xB0, 0x23, 0x3A, 0x7C}; //commands
const unsigned char _l[] PROGMEM = {   2,   10,    6,    4,   18,   12,    2,    2,   32,    6,    4,    2}; //expexted answer length of command
const unsigned char _f[] PROGMEM = {   1,    1,    1,    1,    1,    2,    2,    2,    2,    2,    2,    2}; //format of packet

//wrappers for known commands
const unsigned char _h0[]    PROGMEM = {0x55, 0xAA};
const unsigned char _h1[]    PROGMEM = {0x03, 0x22, 0x01};
const unsigned char _h2[]    PROGMEM = {0x06, 0x20, 0x61};
const unsigned char _end20[] PROGMEM = {0x02, 0x26, 0x22};


const unsigned char BRAKE_RELEASED_TRESHOLD    = 40; //full range (35 -)    range may be different from one to other machines
const unsigned char THROTTLE_RELEASED_TRESHOLD = 43; //full range (38 - 196)
const unsigned char RECV_TIMEOUT =  5;
const unsigned char RECV_BUFLEN  = 64;

//
enum {IN_MOVE = 1, THROTTLE = 2, BRAKE = 4};
union {
  unsigned char Word;
  struct __attribute__((packed))__{
    unsigned char in_move  :1; //speed != 0
    unsigned char throttle :1; //throttle depressed
    unsigned char brake    :1; //brake depressed
  }cc;
}_ControlsState;

struct __attribute__((packed)) ANSWER_HEADER{ //header of receiving answer
  unsigned char len;
  unsigned char addr;
  unsigned char hz;
  unsigned char cmd;
} AnswerHeader;


//55AA   07  20 65   00  04 26 22 00 00 27 FF
struct __attribute__((packed))A20C00HZ65{
  unsigned char hz1;
  unsigned char throttle; //throttle
  unsigned char brake; //brake
  unsigned char hz2;
  unsigned char hz3;
}S20C00HZ65;

struct __attribute__((packed))A25C31{
  unsigned int  remainCapacity;    //remaining capacity mAh
  unsigned char remainPercent;     //charge in percent
  unsigned char u4;                //battery status??? (unknown)
  int           current;           //current        /100 = A
  int           voltage;           //batt voltage   /100 = V
  unsigned char u9;    //2B
  unsigned char u10;   //2C
  //int i4;
}S25C31;

struct __attribute__((packed))A23C3E{
  int i1;                           //mainframe temp
}S23C3E;

struct __attribute__((packed))A23CB0{
  //32 bytes;
  unsigned char u1[10];
  int           speed;              // /1000
  unsigned int  averageSpeed;       // /1000
  unsigned long mileageTotal;       // /1000
  unsigned int  mileageCurrent;     // /100
  unsigned int  currentPowerOnTime; //time from power on, in seconds
  int           mainframeTemp;      // /10
  unsigned char u2[8];
}S23CB0;

struct __attribute__((packed))A23C23{ //skip
  unsigned char u1;
  unsigned char u2;
  unsigned char u3; //0x30
  unsigned char u4; //0x09
  unsigned int  remainMileage;  // /100 
}S23C23;

struct __attribute__((packed))A23C3A{
  unsigned int powerOnTime;
  unsigned int ridingTime;
}S23C3A;
