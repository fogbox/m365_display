#ifndef M365_H
#define M365_H


#include <Arduino.h>
#include <inttypes.h>

class M365{
  public:
    M365(HardwareSerial *);
    void processPacket(unsigned char *, unsigned char); //data *; length
    void writeQuery();
    void processData();
  private:
    HardwareSerial  * port;
    int initialPercent;
    int initialCapacity;
    int spentPercent;
    int spentCapacity;
    int drive;
    int th;
    int br;
    int NewBrThData;
    int Hibernate;


    
    struct __attribute__((packed)) ANSWER_HEADER{ //header of receiving answer
      unsigned char len;
      unsigned char addr;
      unsigned char hz;
      unsigned char cmd;
    }AnswerHeader;



struct __attribute__ ((packed)) {
  unsigned char state;      //0-stall, 1-drive, 2-eco stall, 3-eco drive
  unsigned char ledBatt;    //battery status 0 - min, 7(or 8...) - max
  unsigned char headLamp;   //0-off, 0x64-on
  unsigned char beepAction;
}S21C00HZ64;
struct __attribute__((packed))A20C00HZ65{
  unsigned char hz1;
  unsigned char throttle; //throttle
  unsigned char brake;    //brake
  unsigned char hz2;
  unsigned char hz3;
}S20C00HZ65;
struct __attribute__ ((packed)) A20C00HZ64{
  unsigned char hz1;
  unsigned char throttle; //throttle
  unsigned char brake;    //brake
  unsigned char hz2;
  unsigned char hz3;
  unsigned char hz4;
  unsigned char hz5;
}S20C00HZ64;
struct __attribute__((packed))A25C31{
  unsigned int  remainCapacity;     //remaining capacity mAh
  unsigned char remainPercent;      //charge in percent
  unsigned char u4;                 //battery status??? (unknown)
  int           current;            //current        /100 = A
  int           voltage;            //batt voltage   /100 = V
  unsigned char temp1;              //-=20
  unsigned char temp2;              //-=20
}S25C31;
struct __attribute__((packed))A25C40{ //cells
  int c1; //cell1 /1000
  int c2; //cell2
  int c3; //etc.
  int c4;
  int c5;
  int c6;
  int c7;
  int c8;
  int c9;
  int c10;
  int c11;
  int c12;
  int c13;
  int c14;
  int c15;
}S25C40;
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
  unsigned int  elapsedPowerOnTime; //time from power on, in seconds
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




    
};

struct { //Query
  unsigned char prepared; //1 if prepared, 0 after writing
  unsigned char DataLen;  //lenght of data to write
  unsigned char buf[16];  //buffer
  unsigned int  cs;       //cs of data into buffer
  unsigned char dynQueries[5]; //queries list
  unsigned char dynSize = 0;   //num of q in list
  unsigned char commandLoad;
}Query;

#endif
