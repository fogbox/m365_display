#include "defines.h"

void setup() {
  XIAOMI_PORT.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)

  //display.clearDisplay();
  //display.setTextColor(WHITE);
  //display.setTextSize(2);
  //display.print("BOOT");
  //display.display();
}

void loop() { //cycle time ~170 us
  dataFSM();
  displayFSM();
}


void displayFSM(){
  static unsigned long timer = millis();

  if(millis() - timer < DISPLAY_RENEW_PERIOD){
    return;
  }
  timer = millis();

  unsigned int sph;
  unsigned int spl;
  unsigned int milh; //mileage current
  unsigned int mill;
  unsigned int curh;
  unsigned int curl;
  unsigned int remCharge;
  unsigned int Min;
  unsigned int Sec;
  unsigned int Power; 

  curh = abs(S25C31.current) / 100;     //current
  curl = abs(S25C31.current) % 100;

  sph = abs(S23CB0.speed) / 1000;       //speed
  spl = abs(S23CB0.speed) % 1000 / 100;

  milh = S23CB0.mileageCurrent / 100;   //mileage
  mill = S23CB0.mileageCurrent % 100;

  Min = S23C3A.ridingTime / 60;         //riding time
  Sec = S23C3A.ridingTime % 60;

  remCharge= S25C31.remainPercent;

  display.setTextColor(WHITE);

  display.clearDisplay();

if(S23CB0.speed == 0){
  display.setFont(&FreeSerifBold24pt7b);
  display.setCursor(0,45); 

  display.setTextSize(1);
  display.print("-22.12");
  display.display();
  return;
}


  display.setFont(NULL);
  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0,0);

//speed    
    if(sph < 10){
      display.print(' ');
    }
    display.print(sph);
    display.print('.');
    display.print(spl);
    display.setTextSize(1);
    display.print("k/h ");

//remain charge
    display.setTextSize(2);
    display.print(" ");
    if(remCharge < 100){
      display.print(' ');
    }
    if(remCharge < 10){
      display.print(' ');
    }
    display.print(remCharge);
    display.setTextSize(1);
    display.println('\%');

//second row
    display.setTextSize(2);
    display.setCursor(0,16); //(x, y)

//current mileage
    if(milh < 10){
      display.print(' ');
    }
    display.print(milh);
    display.print('.');
    if(mill < 10){
      display.print('0'); //leading zero
    }

    display.print(mill);
    display.setTextSize(1);
    display.print("km");
    display.setTextSize(2);
    
//third row
    display.setTextSize(2);
    display.setCursor(0,32); //(x, y)

//time
    if(Min < 10){
      display.print(' ');
    }
    display.print(Min);
    display.print(':');
    if(Sec < 10){
      display.print('0');
    }
    display.print(Sec);



//four row
    display.setCursor(0,48); //(x, y)
    if(curh < 10){        //current
      display.print(' ');
    }
    display.print(curh);
    display.print('.');
    if(curl < 10){
      display.print('0');
    }
    display.print(curl);

    display.setTextSize(1);
    display.print("A");
    display.setTextSize(2);

    display.display();

}


void dataFSM(){
  static unsigned char   step = 0, _step = 0, entry = 1;
  static unsigned long   beginMillis;
  static unsigned char   Buf[RECV_BUFLEN];
  static unsigned char * _bufPtr;

  _bufPtr = (unsigned char*)&Buf;

  switch(step){
    case 0:                                                             //search header sequence
      while(XIAOMI_PORT.available() >= 2){
        if(XIAOMI_PORT.read() == 0x55 && XIAOMI_PORT.peek() == 0xAA){
          XIAOMI_PORT.read();                                           //discard second part of header
          step = 1;
          break;
        }
      }
      break;
    case 1: //preamble complete, receive body
      static unsigned char   readCounter;
      static unsigned int    _cs;
      static unsigned char * bufPtr;
      static unsigned char * asPtr; //
      unsigned char bt;
      if(entry){      //init variables
        //DEBUG_PORT.print("hdr: ");
        memset((void*)&AnswerHeader, 0, sizeof(AnswerHeader));
        bufPtr = _bufPtr;
        readCounter = 0;
        beginMillis = millis();
        asPtr   = (unsigned char *)&AnswerHeader;   //pointer onto header structure
        _cs = 0xFFFF;
      }
      if(readCounter >= 127){                       //overrun
        step = 2;
        break;
      }
      if(millis() - beginMillis >= RECV_TIMEOUT){   //timeout
        step = 2;
        break;
      }

      while(XIAOMI_PORT.available()){               //read available bytes from port-buffer
        bt = XIAOMI_PORT.read();
        readCounter++;
        if(readCounter <= sizeof(AnswerHeader)){    //separate header into header-structure
          *asPtr++ = bt;
          _cs -= bt;
        }
        if(readCounter > sizeof(AnswerHeader)){     //now begin read data
          *bufPtr++ = bt;
          if(readCounter < (AnswerHeader.len + 3)){
            _cs -= bt;
          }
        }
        beginMillis = millis();                     //reset timeout
      }

      if(AnswerHeader.len == (readCounter - 4)){    //if len in header == current received len
        unsigned int   cs;
        unsigned int * ipcs;
        ipcs = (unsigned int*)(bufPtr-2);
        cs = *ipcs;
        if(cs != _cs){   //check cs
          //DEBUG_PORT.println("crc_err");
          step = 2;
          break;
        }
        //here cs is ok, header in AnswerHeader, data in _bufPtr
        processPacket(_bufPtr, readCounter);

        step = 2;
        break;
      }
      break; //case 1:
    case 2:  //end of receiving data
      step = 0;
      break;
  }

  if(_step != step){
    _step = step;
    entry = 1;
  }else{
    entry = 0;
  }
}
void processPacket(unsigned char * data, unsigned char len){
  unsigned char RawDataLen;
  unsigned char UnknownPacket = 0;

  RawDataLen = len - sizeof(AnswerHeader) - 2;//(crc)

  switch(AnswerHeader.addr){ //store data into each other structure
    case 0x20: //0x20
      switch(AnswerHeader.cmd){
        case 0x00:
          switch(AnswerHeader.hz){
            case 0x64:
              break;
            case 0x65:
              sendQuery();               //send request from auto-switching list
              break;
            default: //no suitable hz
              UnknownPacket = 1;
              break;
          }
          break;
        case 0x1A:
          break;
        case 0x69:
          break;
        case 0x3E:
          break;
        case 0xB0:
          break;
        case 0x23:
          break;
        case 0x3A:
          break;
        case 0x7C:
          break;
        default: //no suitable cmd
          UnknownPacket = 1;
          break;
      }
      break;
    case 0x21:
      switch(AnswerHeader.cmd){
        case 0x00:
          break;
      default:
        UnknownPacket = 1;
        break;
      }
      break;
    case 0x22:
      switch(AnswerHeader.cmd){
        case 0x3B:
          break;
        case 0x31:
          break;
        case 0x20:
          break;
        case 0x1B:
          break;
        case 0x10:
          break;
        default:
          UnknownPacket = 1;
          break;
      }
      break;
    case 0x23:
      switch(AnswerHeader.cmd){
        case 0x1A:
          //if(RawDataLen == sizeof(A23C1A)){
          //  memcpy((void*)& S23C1A, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C1A_OK");
          //}
          break;
        case 0x69:
          //if(RawDataLen == sizeof(A23C69)){
          //  memcpy((void*)& S23C69, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C69_OK");
          //}
          break;
        case 0x3E:
          if(RawDataLen == sizeof(A23C3E)){
            memcpy((void*)& S23C3E, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C3E_OK");
          }
          break;
        case 0xB0:
          if(RawDataLen == sizeof(A23CB0)){
            memcpy((void*)& S23CB0, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23CB0_OK");
          }
          break;
        case 0x23:
          if(RawDataLen == sizeof(A23C23)){
            memcpy((void*)& S23C23, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C23_OK");
          }
          break;
        case 0x3A:
          if(RawDataLen == sizeof(A23C3A)){
            memcpy((void*)& S23C3A, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C3A_OK");
          }
          break;
        case 0x7C:
          //if(RawDataLen == sizeof(A23C7C)){
          //  memcpy((void*)& S23C7C, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C7C_OK");
          //  }
            break;
        case 0x7B:
          break;
        case 0x17:
          break;
        case 0x7D:
          break;
        default:
          UnknownPacket = 1;
          break;
      }
      break;          
    case 0x25:    
      switch(AnswerHeader.cmd){
        case 0x3B:
          //if(RawDataLen == sizeof(A25C3B)){
          //  memcpy((void*)& S25C3B, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C3B_OK");
          //}
          break;
        case 0x31:
          if(RawDataLen == sizeof(A25C31)){
            memcpy((void*)& S25C31, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C31_OK");
          }
          break;
        case 0x20:
          //if(RawDataLen == sizeof(A25C20)){
          //  memcpy((void*)& S25C20, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C20_OK");
          //}
          break;
        case 0x1B:
          //if(RawDataLen == sizeof(A25C1B)){
          //  memcpy((void*)& S25C1B, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C1B_OK");
          //}
          break;
        case 0x10:
          //if(RawDataLen == sizeof(A25C10)){
          //  memcpy((void*)& S25C10, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C10_OK");
          //}
          break;
        default:
          UnknownPacket = 1;
          break;
        }
        break;
      default:
        UnknownPacket = 1;
        break;
  }

  if(UnknownPacket == 1){
    
  }

}
void sendQuery(){
 static unsigned long RequestTime = millis();
  if(millis() - RequestTime <= 100){ 
    return;
  }
unsigned char buf[16];
unsigned char * ptrBuf;
unsigned char *ptrData;
unsigned char len = 0;
unsigned int  cs;
static unsigned char var = 0;  //variant of query, static
static unsigned char endEnable = 0;


endEnable = 0;
len = 0;

static unsigned char h0[] = {0x55, 0xAA};
static unsigned char h1[] = {0x03, 0x22, 0x01};
static unsigned char h2[] = {0x06, 0x20, 0x61};

static unsigned char end20[]  = {0x02, 0x26, 0x22};


//battery information
static unsigned char q1[] = {0x3B, 0x02};
static unsigned char q2[] = {0x31, 0x0A};                //+++
static unsigned char q3[] = {0x20, 0x06};
static unsigned char q4[] = {0x1B, 0x04};
static unsigned char q5[] = {0x10, 0x12};

//basic info
static unsigned char q6[] = {0x1A, 0x0C};   //end20_22
static unsigned char q7[] = {0x69, 0x02};   //end20_22
static unsigned char q8[] = {0x3E, 0x02};   //end20_22

//main vindow        
static unsigned char q9[] = {0xB0, 0x20};   //end20_22   //+++
static unsigned char q10[]= {0x23, 0x06};   //end20_22
static unsigned char q11[]= {0x3A, 0x04};   //end20_22   //+++
static unsigned char q12[]= {0x7C, 0x02};   //end20_22


  ptrBuf  = (unsigned char*)&buf;
  ptrData = (unsigned char*)&h0;

  var++;
  if(var > 11){  //reset var
    var = 0;
  }

  for(int i = 2; i > 0 ;i--){ //copy h0
    *ptrBuf++ = *ptrData++;
  }

  switch(var){
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
      ptrData = (unsigned char *)&h1;
      break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
      endEnable = 1;
      ptrData = (unsigned char *)&h2;
      break;
  }

  for(int i = 3; i > 0 ;i--){ //copy h1 or h2
    *ptrBuf++ = *ptrData++;
    len++;
  }

  switch(var){
    case 0:
      return;
      ptrData = (unsigned char*)&q1;
      break;
    case 1:
      ptrData = (unsigned char*)&q2; //+++
      break;
    case 2:
      return;
      ptrData = (unsigned char*)&q3;
    break;
    case 3:
      return;
      ptrData = (unsigned char*)&q4;
    break;
    case 4:
      return;
      ptrData = (unsigned char*)&q5;
    break;
    case 5:
      return;
      ptrData = (unsigned char*)&q6;
      //endEnable = 1;
    break;
    case 6:
      return;
      ptrData = (unsigned char*)&q7;
      //endEnable = 1;
    break;
    case 7:
      return;
      ptrData = (unsigned char*)&q8;
      //endEnable = 1;
    break;
    case 8:
      ptrData = (unsigned char*)&q9; //+++
      //endEnable = 1;
    break;
    case 9:
      return;
      ptrData = (unsigned char*)&q10;
      //endEnable = 1;
    break;
    case 10:
      ptrData = (unsigned char*)&q11; //+++
      //endEnable = 1;
    break;
    case 11:
      return;
      ptrData = (unsigned char*)&q12;
      //endEnable = 1;
    break;
    default:
      return;
      //DEBUG_PORT.print("ERROR");
      return;
    break;
  }

  for(int i = 2; i > 0 ;i--){ //copy body
    *ptrBuf++ = *ptrData++;
    len++;
  }

  static unsigned char * endPtr;
  endPtr = (unsigned char*)&end20;

  if(endEnable == 1){
    for(int i = 3; i > 0 ;i--){ //copy ender (if needed)
      *ptrBuf++ = *endPtr++;
      len++;
    }
  }
  static unsigned char * pcs;
  pcs = (unsigned char*) &cs;

  cs = calcCs((unsigned char*)&buf[2], len);

  *ptrBuf++ = *pcs++;  //copy cs into buf
  *ptrBuf++ = *pcs++;
   len+=4; //header + cs

  UCSR0B &= ~_BV(RXEN0);            //send request from auto-switching list
  XIAOMI_PORT.write((unsigned char*)buf, len); 
  UCSR0B |= _BV(RXEN0);

  RequestTime = millis();
}
unsigned int calcCs(unsigned char * data, unsigned char len){
  unsigned int cs = 0xFFFF;
  for(int i = len; i > 0; i--){
    cs -= *data++;
  }
  return cs;
}


