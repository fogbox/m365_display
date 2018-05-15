#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
//#include <./Fonts/FreeSerifBold24pt7b.h>
//#include <gfxfont.h>

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif


#define XIAOMI_PORT Serial

const unsigned char RECV_TIMEOUT =  5;
const unsigned char RECV_BUFLEN  = 64;
const unsigned char DISPLAY_RENEW_PERIOD =  200;

//структура заголовка ответа принимаемого от контроллера
struct __attribute__((packed)) ANSWER_HEADER{ //header of receiving answer
  unsigned char len;
  unsigned char addr;
  unsigned char hz;
  unsigned char cmd;
} AnswerHeader;


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

//0100
//6500

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
