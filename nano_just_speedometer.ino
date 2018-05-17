#include "defines.h"


void setup() {
  XIAOMI_PORT.begin(115200);

nextQuery();
nextQuery();
nextQuery();
nextQuery();

  
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(2);
  display.print("BOOT");
  display.display();
  delay(200);

  _newDataFlag = 1;
}

void loop() { //cycle time w\o data exchange ~400 us
  /*
  static unsigned long timer = 0;
  static unsigned int  counter = 0; //perfomance counter
  counter++;

  if(millis() - timer >= 1000){
    timer = millis();
    Serial.println(counter);
    counter = 0;
  }
  */
  
  dataFSM();


  if(_newDataFlag == 1){
    _newDataFlag = 0;
    displayFSM();
  }
}

void displayFSM(){

struct {
  unsigned int sph;
  unsigned int spl;
  unsigned int milh; //mileage current
  unsigned int mill;
  unsigned int curh;
  unsigned int curl;
  unsigned int remCharge;
  unsigned int Min;
  unsigned int Sec;
}D;

  D.curh = abs(S25C31.current) / 100;     //current
  D.curl = abs(S25C31.current) % 100;

  D.sph = abs(S23CB0.speed) / 1000;       //speed
  D.spl = abs(S23CB0.speed) % 1000 / 100;

  D.milh = S23CB0.mileageCurrent / 100;   //mileage
  D.mill = S23CB0.mileageCurrent % 100;

  D.Min = S23C3A.ridingTime / 60;         //riding time
  D.Sec = S23C3A.ridingTime % 60;

  D.remCharge= S25C31.remainPercent;

  display.setTextColor(WHITE);
  display.clearDisplay();


  if(S23CB0.speed > 10){
   display.setFont(&FreeSerifBold24pt7b);
   display.setCursor(0,45); 

//current
   if(S25C31.current < 0){
     display.print('-');
   }
 
   display.setTextSize(1);
  if(D.curh < 10){
    display.print(' ');
  }
   display.print(D.curh);
   display.print('.');
   if(D.curl < 10){
    display.print('0');
   }
   display.print(D.curl);
   display.display();
   return;
  }



  display.setFont(NULL);
  display.setTextSize(2);
  display.clearDisplay();
  display.setCursor(0,0);

//speed    
    if(D.sph < 10){
      display.print(' ');
    }
    display.print(D.sph);
    display.print('.');
    display.print(D.spl);
    display.setTextSize(1);
    display.print("k/h ");

//remain charge
    display.setTextSize(2);
    display.print(" ");
    if(D.remCharge < 100){
      display.print(' ');
    }
    if(D.remCharge < 10){
      display.print(' ');
    }
    display.print(D.remCharge);
    display.setTextSize(1);
    display.println('\%');

//second row
    display.setTextSize(2);
    display.setCursor(0,16); //(x, y)

//current mileage
    if(D.milh < 10){
      display.print(' ');
    }
    display.print(D.milh);
    display.print('.');
    if(D.mill < 10){
      display.print('0'); //leading zero
    }

    display.print(D.mill);
    display.setTextSize(1);
    display.print("km");
    display.setTextSize(2);
    
//third row
    display.setTextSize(2);
    display.setCursor(0,32); //(x, y)

//time
    if(D.Min < 10){
      display.print(' ');
    }
    display.print(D.Min);
    display.print(':');
    if(D.Sec < 10){
      display.print('0');
    }
    display.print(D.Sec);



//four row
    display.setCursor(0,48); //(x, y)
    if(D.curh < 10){        //current
      display.print(' ');
    }
    display.print(D.curh);
    display.print('.');
    if(D.curl < 10){
      display.print('0');
    }
    display.print(D.curl);

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
        memset((void*)&AnswerHeader, 0, sizeof(AnswerHeader));
        bufPtr = _bufPtr;
        readCounter = 0;
        beginMillis = millis();
        asPtr   = (unsigned char *)&AnswerHeader;   //pointer onto header structure
        _cs = 0xFFFF;
      }
      if(readCounter >= RECV_BUFLEN){                       //overrun
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
  static unsigned char blePackCounter = 0; //counter for [07 20 65] BLE reports
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
              blePackCounter++;
              if(blePackCounter >= 5){     //request after every 5 ble report FIXME: magicword
                nextQuery();
                blePackCounter = 0;
              }
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
      _newDataFlag = 1;
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
        case 0x3E: //mainframe temperature
          if(RawDataLen == sizeof(A23C3E)){
            memcpy((void*)& S23C3E, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C3E_OK");
          }
          break;
        case 0xB0: //speed, average speed, mileage total, mileage current, power on time, mainframe temp
          if(RawDataLen == sizeof(A23CB0)){
            memcpy((void*)& S23CB0, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23CB0_OK");
          }
          break;
        case 0x23: //remain mileage
          if(RawDataLen == sizeof(A23C23)){
            memcpy((void*)& S23C23, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A23C23_OK");
          }
          break;
        case 0x3A: //power on time, riding time
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
      _newDataFlag = 1;
      switch(AnswerHeader.cmd){
        case 0x3B:
          //if(RawDataLen == sizeof(A25C3B)){
          //  memcpy((void*)& S25C3B, (void*)data, RawDataLen);
            //DEBUG_PORT.println("A25C3B_OK");
          //}
          break;
        case 0x31: //capacity, remain persent, current, voltage
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
static unsigned char buf[16];
unsigned char * ptrBuf;
unsigned char *ptrData;
unsigned char len = 0;
unsigned int  cs;
unsigned char endEnable = 0;

static unsigned char var = 0;  //variant of query, static

static unsigned char h0[] = {0x55, 0xAA};
static unsigned char h1[] = {0x03, 0x22, 0x01};
static unsigned char h2[] = {0x06, 0x20, 0x61};

static unsigned char end20[]  = {0x02, 0x26, 0x22};

//unsigned char q[] = {0x3B, 0x31, 0x20, 0x1B, 0x10, 0x1A, 0x69, 0x3E, 0xB0, 0x23, 0x3A, 0x7C};
//unsigned char l[] = {   2,   10,    6,    4,   18,   12,    2,    2,   32,    6,    4,    2};


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

  switch(var){   //select body
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

  UCSR0B &= ~_BV(RXEN0);            
  XIAOMI_PORT.write((unsigned char*)buf, len); //send request
  UCSR0B |= _BV(RXEN0);

}
unsigned int calcCs(unsigned char * data, unsigned char len){
  unsigned int cs = 0xFFFF;
  for(int i = len; i > 0; i--){
    cs -= *data++;
  }
  return cs;
}

void nextQuery(){ //select next command from aray and send
  static unsigned char index = 0;

  sendQueryFromTable(_commandsWeWillSend[index]);

  index++;
  if(index >= sizeof(_commandsWeWillSend)){
    index = 0;
  }
}

void sendQueryFromTable(unsigned char index){
  static unsigned char buf[16];
  unsigned char * ptrBuf;
  unsigned int    cs;

  unsigned char * pp; //pointer preamble
  unsigned char * ph; //pointer header
  unsigned char * pe; //pointer end

  unsigned char cmdFormat;
  unsigned char hLen; //header length
  unsigned char eLen; //ender length

  cmdFormat = pgm_read_byte_near(_f + index);

  pp = (unsigned char*)&_h0;
  ph = NULL;
  pe = NULL;

  switch(cmdFormat){
    case 1: //h1 only
      ph = (unsigned char*)&_h1;
      hLen = sizeof(_h1);
      pe = NULL;
      break;
    case 2: //h2 + end20
      ph = (unsigned char*)&_h2;
      hLen = sizeof(_h2);
      pe = (unsigned char*)&_end20;
      eLen = sizeof(_end20);
      break;
  }

  ptrBuf = (unsigned char*)&buf;

  memcpy_P((void*)ptrBuf, (void*)pp, sizeof(_h0));  //copy preamble
  ptrBuf += sizeof(_h0);

  
  memcpy_P((void*)ptrBuf, (void*)ph, hLen);         //copy header
  ptrBuf += hLen;
  
  memcpy_P((void*)ptrBuf, (void*)(_q + index), 1);  //copy query
  ptrBuf++;
  
  memcpy_P((void*)ptrBuf, (void*)(_l + index), 1);  //copy expected answer length
  ptrBuf++;

  if(pe != NULL){
    memcpy_P((void*)ptrBuf, (void*)pe, eLen);       //if needed - copy ender
    ptrBuf+= hLen;
  }

  unsigned char DataLen = ptrBuf - (unsigned char*)&buf[2]; //calculate length of data in buf

  cs = calcCs((unsigned char*)&buf[2], DataLen);    //calculate cs of buffer

  //send request
  UCSR0B &= ~_BV(RXEN0);
  XIAOMI_PORT.write((unsigned char*)&buf, DataLen + 2);     //DataLen + length of cs
  XIAOMI_PORT.write((unsigned char*)&cs, 2);
  UCSR0B |= _BV(RXEN0);
}
