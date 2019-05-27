#include "defines.h"

volatile unsigned long loopsCounter;
volatile unsigned long loopsTime;

/*
 * screen1 //brake
 *  distance
 *  percent cost
 *  capacity cost
 *  time of ride
 *  time power on
 * 
 * screen2 //throttle
 *  remain percent
 *  remain capacity
 *  B-temp 1
 *  B-temp 2
 * 
 * screen3 //both
 *  current
 *  temperature
 *  total mileage
 */

void setup() {

  SoftwareSerial BMSport(2,3);
  
  XIAOMI_PORT.begin(115200);
  u8g2.begin();

  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_ncenB14_tr);
    u8g2.drawStr(10,20, "Hello Kate!");
    u8g2.drawStr(17,60,"- no BUS -");
  } while (u8g2.nextPage());

  delay(200);

#ifdef SCR_TESTING
    _Menu.stallVar = DV_SCR_1;
  #else
    initFromEep();
  #endif
}

void initFromEep(){
  Menu.bigVar   = eeprom_read_byte((const unsigned char  *)0);
  Menu.stallVar = eeprom_read_byte((const unsigned char  *)2);
  D.odo1_init    = eeprom_read_dword((const unsigned long *)3);
  D.odo2_init    = eeprom_read_dword((const unsigned long *)7);
}

void loop() {
  static unsigned long _millis = 0;
  static unsigned long counter = 0;
  static unsigned long dispMillis = 0;

  counter ++;
  if(millis() - _millis >= 1000){
    _millis = millis();
    loopsCounter = counter;
    counter = 0;
  }

  if(_Hibernate == 1){
  }

#ifdef SCR_TESTING
  _Menu.stallVar = _Menu.dispVar = DV_SCR_1;
#endif

  dataFSM();            //communication with m365
  keyProcessFSM();      //tracks throttle and brake and send messages for menuControlFSM()
  menuControlFSM();     //switch menu variables and prepares commands (cruise, led...)
  screenSwitcherFSM();  //control display

  if(Message.GetBroadcast(BR_MESSAGE_HIBERNATE_ON)){
    _Hibernate = 1;
  }
  if(Message.GetBroadcast(BR_MESSAGE_HIBERNATE_OFF)){
    _Hibernate = 0;
  }

  if(Query.prepared == 0 && XIAOMI_PORT.available() < 5 && _NewBrThData == 1){
    prepareNextQuery();
  }

  if(_NewDataFlag == 1){
    calculate();
    _NewDataFlag = 0;
  }

  if(millis() - dispMillis >= 200){
    dispMillis = millis();
    displayRoutine(Menu.dispVar); //takes about 60ms.

    if(Query.commandLoad == 0){   //for avoid overwriting not yet sent command in [Query]
      Query.prepared = 0;
    }
    _NewBrThData = 0;
    flushRxBuffer();
    Message.Post(MSG_DATA_FSM_RESET);
  }

  Message.Process();
  Message.ProcessBroadcast();
}

void screenSwitcherFSM(){ //switch screen between stall and run (BIG_DIGITS) mode
 static unsigned char step = 0;
 static unsigned char dispVar;

  if(Message.GetBroadcast(BR_MESSAGE_HIBERNATE_ON)){
    dispVar = Menu.dispVar;
    Menu.dispVar = DV_HIBERNATE;
  }

  if(Message.GetBroadcast(BR_MESSAGE_HIBERNATE_OFF)){
    Menu.dispVar = dispVar;
  }

  if(Menu.dispVar == DV_MENU || Menu.dispVar == DV_HIBERNATE){    //switching not needed, return
    return;
  }

  switch(step){
    case 0: //stop
      if(Message.Get(MESSAGE_KEY_BOTH)){
      }
      if(Message.Get(MESSAGE_KEY_TH)){  //ALL MESSAGES ARE SENT IF SPEED < 1 kmh ONLY!!!
        Menu.stallVar++;
      }
      if(Message.Get(MESSAGE_KEY_BR)){
        Menu.stallVar--;
      }
      if(Menu.stallVar > DV_SCR_1){    //switch up
        Menu.stallVar  = DV_SCR_1;
      }
      if(Menu.stallVar < DV_SCR_1){    //switch screens down
        Menu.stallVar  = DV_SCR_1;
      }
      if(Menu.dispVar != Menu.stallVar){
        //eeprom_write_byte((unsigned char *)2, Menu.stallVar);
        Menu.dispVar = Menu.stallVar;  
      }
/*
      if(D.sph == 0 && D.spl == 0 && S25C31.current < 0){ //go into charging screen
        _Menu.dispVar = DV_CHARGING;
        step = 1;
      }
*/
      if(D.drive == 1){
        if(Menu.bigVar == DV_NO_BIG){
          step = 10;
          break;
        }
        dispVar = Menu.dispVar;            //store current display
        Menu.dispVar = Menu.bigVar;       //switch to BIG
        step = 10;
      }
      break;
    case 1: //charging
      if(Message.Get(MESSAGE_KEY_BOTH)){    //await ends of charging
        Menu.dispVar = DV_SCR_4; //print CELLS
      }
      if(Message.Get(MESSAGE_KEY_BR)){
        Menu.dispVar = DV_CHARGING;
      }
      if(S25C31.current >= 0){              //return from charging
        step = 10;
      }
      break;
    default:
      if(D.drive == 0){
        Menu.dispVar = dispVar;
        step = 0;
      }
      break;
  }
}

//terrible menu
void menuControlFSM(){    //store display mode, switch on display menu, accept messages, switch menu, react on select menu items (long keys)
  static unsigned char step = 0;
  static unsigned long timer = 0;
  static unsigned char dispVar;

  switch(step){
    case 0:
      if(Message.Get(MESSAGE_KEY_MENU)){ //enter into menu
        timer = millis();
        dispVar = Menu.dispVar;
        Menu.dispVar = DV_MENU;
        Menu.selItem = MM_BIG;
        step = 1;
      }
      break;
    case 1:                                                           //********* MAIN MENU HERE
      if(millis() - timer >= 3000){ //exit from menu via timeout
        step = 2;
        break;
      }

      if(D.sph > 0){                //exiting from menu if speed increasing
        step = 2;
        break;
      }

      if(Message.Get(MESSAGE_KEY_BR)){    
        timer = millis();               //renew timeout       
        switch(Menu.activeMenu){
          case MM_MAIN:
            Menu.selItem -= 1;
            if(Menu.selItem == 0){
              Menu.selItem = 6;
            }
            break;
          case MM_BIG:
            Menu.selItem -= 1;
            if(Menu.selItem == 0){
              Menu.selItem = 10;
            }
            break; 
        }
      }

      if(Message.Get(MESSAGE_KEY_TH)){
        timer = millis();
        switch(Menu.activeMenu){       //scrolling main menu
          case MM_MAIN:
            Menu.selItem++;
            if(Menu.selItem > 6){
              Menu.selItem = 1;
            }
            break;
          case MM_RECUP: //recup menu
            Menu.selItem ++;
            if(Menu.selItem > 3){
              Menu.selItem = 1;
            }
            break;
          case MM_CRUISE: //cruise menu
            Menu.selItem ++;
            if(Menu.selItem > 2){
              Menu.selItem = 1;
            }
            break;
          case MM_RLED: //led menu
            Menu.selItem ++;
            if(Menu.selItem > 2){
              Menu.selItem = 1;
            }
            break;
          case MM_BIG: //big display menu
            Menu.selItem ++;
            if(Menu.selItem > 10){
              Menu.selItem = 1;
            }
            break;
          case MM_ODO: //odo
            Menu.selItem ++;
            if(Menu.selItem > 2){
              Menu.selItem = 1;
            }
            break;
        }
      }

      if(Message.Get(MESSAGE_KEY_TH_LONG)){   //get seleted item and send appropriate command
        timer = millis();
        switch(Menu.activeMenu){
          case MM_MAIN: //main menu
            switch(Menu.selItem){            //go into submenu
              case MM_RECUP: //sel recup
                Menu.selItem = 1;
                Menu.activeMenu = MM_RECUP;
                break;
              case MM_CRUISE: //sel cruise
                Menu.selItem = 1;
                Menu.activeMenu = MM_CRUISE;
                break;
              case MM_RLED: //sel led
                Menu.selItem = 1;
                Menu.activeMenu = MM_RLED;
                break;
              case MM_BIG: //sel big data
                Menu.selItem = (Menu.bigVar - DV_BIG_SPEED) + 1;
                /*
                switch(_Menu.bigVar){
                  case DV_BIG_SPEED:
                    _Menu.selItem = 1;
                    break;
                  case DV_BIG_AVERAGE:
                    _Menu.selItem = 2;
                    break;
                  case DV_BIG_CURRENT:
                    _Menu.selItem = 3;
                    break;
                  case DV_BIG_SPENT:
                    _Menu.selItem = 4;
                    break;
                  case DV_BIG_MILEAGE:
                    _Menu.selItem = 5;
                    break;
                  case DV_BIG_VOLTS:
                    _Menu.selItem = 6;
                    break;
                  case DV_BIG_TIME:
                    _Menu.selItem = 7;
                    break;
                  case DV_NO_BIG:
                    _Menu.selItem = 8;
                    break;
                  default:
                    _Menu.selItem = 1;
                    break;
                }
                */
                Menu.activeMenu = MM_BIG;
                break;
              case MM_HIBERNATE: //hibernate
                if(_Hibernate == 0){
                  Message.PostBroadcast(BR_MESSAGE_HIBERNATE_ON);
                }else{
                  Message.PostBroadcast(BR_MESSAGE_HIBERNATE_OFF);
                }
                step = 2;
                break;
              case MM_ODO: //odo
                Menu.selItem = 1;
                Menu.activeMenu = MM_ODO;
                break;
            }
            break;
          case MM_RECUP: //recup menu
            switch(Menu.selItem){
              case 1: //weak
                prepareCommand(CMD_WEAK);
                step = 2;
                break;
              case 2: //medium
                prepareCommand(CMD_MEDIUM);
                step = 2;
                break;
              case 3: //strong
                prepareCommand(CMD_STRONG);
                step = 2;
                break;
            }
            break;
          case MM_CRUISE:  //cruise menu
            switch(Menu.selItem){
              case 1: //on
                prepareCommand(CMD_CRUISE_ON);
                step = 2;
                break;
              case 2: //off
                prepareCommand(CMD_CRUISE_OFF);
                step = 2;
                break;
            }
            break;
          case MM_RLED:  //led menu
            switch(Menu.selItem){
              case 1: //on
                prepareCommand(CMD_LED_ON);
                step = 2;
                break;
              case 2: //off
                prepareCommand(CMD_LED_OFF);
                step = 2;
                break;
            }
            break;
          case MM_BIG:  //big menu dependencies
            Menu.bigVar = Menu.selItem - 1 + DV_BIG_SPEED;
            Menu.bigVar = constrain(Menu.bigVar, (int)DV_BIG_SPEED, (int)DV_NO_BIG);
            /*
            switch(_Menu.selItem){
              case 1: //speed
                _Menu.bigVar = DV_BIG_SPEED;
                break;
              case 2: //current
                _Menu.bigVar = DV_BIG_AVERAGE;
                break;
              case 3: //average speed
                _Menu.bigVar = DV_BIG_CURRENT;
                break;
              case 4:
                _Menu.bigVar = DV_BIG_SPENT;
                break;
              case 5:
                _Menu.bigVar = DV_BIG_MILEAGE;
                break;
              case 6:
                _Menu.bigVar = DV_BIG_VOLTS;
                break;
              case 7:
                _Menu.bigVar = DV_BIG_TIME;
                break;
              case 8:
                _Menu.bigVar = DV_NO_BIG;
                break;
              case 9:
                _Menu.bigVar = DV_BIG_ODO1;
                break;
            }
            */
            eeprom_write_byte((unsigned char*)0, Menu.bigVar); //store
            step = 2;
            break;
          case MM_ODO:  //ODO reset
            switch(Menu.selItem){
              case 1:
                eeprom_write_dword((unsigned long *)3, S23CB0.mileageTotal);
                break;
              case 2:
                eeprom_write_dword((unsigned long *)7, S23CB0.mileageTotal);
                break;
            }
            initFromEep();
            step = 2;
            break;
        }
      }

      if(Message.Get(MESSAGE_KEY_BOTH)){
        timer = millis();
      }

      if(Message.Get(MESSAGE_KEY_MENU)){      //long both - exit from menu
        timer = millis();
        break;
      }

      if(Message.Get(MESSAGE_KEY_BR_LONG)){   //exit from menu
        timer = millis();
        switch(Menu.activeMenu){
          case 0:                       //if already main menu
            step = 2;                   //exit from menu
            break;
          default:
            Menu.activeMenu = 0;       //one level up
            Menu.selItem = 1;
        }

        break;
      }
      break;
    case 2: //exit from menu
      Menu.activeMenu = 0;
      Menu.selItem = 0;
      Menu.dispVar = dispVar;
      step = 0;
      break;
  }
}

void calculate(){
  static unsigned char init = 0;
  switch(AnswerHeader.cmd){  //1 8 10 14
    case 0x31: // 1
      D.curh = abs(S25C31.current) / 100;     //current
      D.curl = abs(S25C31.current) % 100;
      D.remCapacity = S25C31.remainCapacity;
      D.remPercent= S25C31.remainPercent;
      D.temp1 = S25C31.temp1 - 20;
      D.temp2 = S25C31.temp2 - 20;
      D.voltage = S25C31.voltage;
      D.volth = S25C31.voltage / 100;
      D.voltl = S25C31.voltage % 100;
      D.spentPercent  = abs(D.initialPercent  - D.remPercent);
      D.spentCapacity = abs(D.initialCapacity - D.remCapacity);
      break;
    case 0xB0: // 8 speed average mileage poweron time
      D.sph = abs(S23CB0.speed) / 1000;       //speed
      D.spl = abs(S23CB0.speed) % 1000 / 10;
      D.milh = S23CB0.mileageCurrent / 100;   //mileage
      D.mill = S23CB0.mileageCurrent % 100;
      D.milTotH = S23CB0.mileageTotal / 1000; //mileage total
      D.milTotL = S23CB0.mileageTotal % 10;
      D.aveh = S23CB0.averageSpeed / 1000;
      D.avel = S23CB0.averageSpeed % 1000;
      D.odo1 = S23CB0.mileageTotal - D.odo1_init;
      D.odo2 = S23CB0.mileageTotal - D.odo2_init;
      break;
    case 0x3A: //10
      D.tripMin  = S23C3A.ridingTime  / 60;   //riding time
      D.tripSec  = S23C3A.ridingTime  % 60;
      D.powerMin = S23C3A.powerOnTime / 60;   //power on time
      D.powerSec = S23C3A.powerOnTime % 60;
      break;
    case 0x40: //cells
      break;
    default:
      break;
  }

  if(init == 0){ //initialise vars, only once after power up and get data
    if(D.remCapacity != 0 && D.remPercent != 0){
      D.initialPercent = D.remPercent;
      D.initialCapacity = D.remCapacity;
      init = 1;
    }
  }
}

void keyProcessFSM(){ //track throttle and brake and post messages (MENU, TH, BR, BOTH, LONG_TH, LONG_BR)
  enum {NO = 0, BR = 1, TH = 2, BOTH = 3};  
  static unsigned char step = 0;
  static unsigned long timer = millis();
  static unsigned char key = NO, _key;
  
  if(D.drive != 0){        //keys operates at stopped machine only
    step = 0;
  }
  
  if(D.br >= BR_KEY_TRES){ //detect keys
    key |= BR;
  }else{
    key &= ~BR;
  }
  if(D.th >= TH_KEY_TRES){
    key |= TH;
  }else{
    key &= ~TH;
  }

  switch(step){
    case 0:             //waiting for release both keys
      if(D.drive == 0 &&  key == NO){
        if(millis() - timer >= MENU_INITIAL){
          _key = NO;
          step = 1;
        }
      }else{
        timer = millis();
      }
      break;
    case 1:             //waiting for any key
      if(key != NO){    //depressed any key - go into next step
        timer = millis();
        step = 2;
      }
      break;
    case 2:             //wait for release both keys
      if(key != NO){
        _key |= key;
      }
      if(millis() - timer >= LONG_PRESS){
        switch(_key){
          case BOTH:
            Message.Post(MESSAGE_KEY_MENU);
            break;
          case TH:
            Message.Post(MESSAGE_KEY_TH_LONG);
            break;
          case BR:
            Message.Post(MESSAGE_KEY_BR_LONG);
            break;
        }
        step = 0;
        break;
      }
      if(key == NO){    //it`s short press
        switch(_key){
          case TH:
            Message.Post(MESSAGE_KEY_TH);
            break;
          case BR:
            Message.Post(MESSAGE_KEY_BR);
            break;
          case BOTH:
            Message.Post(MESSAGE_KEY_BOTH);
            break;
        }
        step = 0;
        break;
      }
  }
}

void displayRoutine(unsigned char disp_variant){
  if(disp_variant == DV_MENU){
    switch(Menu.activeMenu){
      case MM_MAIN: //main
        printMenu(Menu.selItem, menuMainItems);
        break;
      case MM_BIG: //big
        printMenu(Menu.selItem, menuBigItems);
        break;
      case MM_ODO: //ODO reset
        printMenu(Menu.selItem, menuOdoItems);
        break;          
      case MM_RECUP: //recup
        printMenu(Menu.selItem, menuRecupItems);
        break;
      case MM_CRUISE: //cruise
        printMenu(Menu.selItem, menuOnOffItems);
        break;
      case MM_RLED: //led
        printMenu(Menu.selItem, menuOnOffItems);
        break;
      default:
        break;
    }
    return;
  }

  switch(disp_variant){
    case DV_SCR_1:
      print_display_1();
      break;
    case DV_SCR_2:
      print_display_2();
      break;
    case DV_SCR_3:
      print_display_3();
      break;
    case DV_SCR_4:
      print_display_4();
      break;
    case DV_SCR_5:
      print_display_5();
      break;

    case DV_HIBERNATE:
      break;
    case DV_BIG_TIME:
      unsigned int tTime;
      tTime  = D.powerMin * 100;
      tTime += D.powerSec;
      printBig(tTime, dispBp[5]);
      break;   
    case DV_BIG_VOLTS:
      printBig(S25C31.voltage, dispBp[0]);
      break;
    case DV_BIG_AVERAGE:
      printBig(S23CB0.averageSpeed / 10, dispBp[1]);
      break;
    case DV_BIG_CURRENT:                               //-- BIG CURRENT
      printBig(S25C31.current, dispBp[2]);
      break;
    case DV_BIG_MILEAGE:                              //-- BIG MILEAGE
      printBig(S23CB0.mileageCurrent, dispBp[3]);
      break;
    case DV_BIG_SPEED:                                 //-- BIG SPEED
      printBig(S23CB0.speed / 10, dispBp[4]);
      break;

    case DV_CHARGING:
      break;

    case DV_BIG_SPENT:                                 //-- BIG SPENT
      printBig(D.spentPercent * 100, bp7);// dispBp[4]);
      break;

    case DV_NONE: //U can`t touch this :)
      return;
      break;
      
    case DV_BIG_ODO1:
      printBig(D.odo1 / 10, dispBp[6]);
      break;

    default:
      break;
  }
}

void dataFSM(){
  static unsigned char   step = 0;
  static unsigned long   beginMillis;
  static unsigned char   buf[RECV_BUFLEN];
  static unsigned char   readCounter;
  static unsigned int    calcCs;
  static unsigned char * bufPtr;
  static unsigned char * asPtr;
  unsigned char bt;

  if(Message.Get(MSG_DATA_FSM_RESET)){    //"manual" reset
    step = 0;
  }

  if(millis() - beginMillis >= RECV_TIMEOUT){ //reset FSM if timeout
    step = 0;
  }

  switch(step){
    case 0:                                                         
      while(XIAOMI_PORT.available() >= 2){  
        if(XIAOMI_PORT.read() == 0x55 && XIAOMI_PORT.peek() == 0xAA){ //search preamble  
          XIAOMI_PORT.read();     //discard second part of header
          beginMillis = millis(); //reset timeout
          //init
          memset((void*)&AnswerHeader, 0, sizeof(AnswerHeader));
          bufPtr = (unsigned char *) &buf;
          readCounter = 0;
          beginMillis = millis();
          asPtr   = (unsigned char *)&AnswerHeader;   //pointer onto header structure
          calcCs = 0xFFFF;
          step = 1;
          break;
        }
      }
      break;
    case 1://header
      if(XIAOMI_PORT.available() < (int)sizeof(AnswerHeader)){
        break;
      }
      for(int i = 0; i < (int)sizeof(AnswerHeader); i++){  //copy all header
        bt = XIAOMI_PORT.read();
        *asPtr++ = bt;
        readCounter++;
        calcCs -= bt;
      }
      step = 2;
      break;
    case 2: //body
      if(AnswerHeader.len >= RECV_BUFLEN){
        step = 0;
        break;
      }
      if(XIAOMI_PORT.available() < (int)AnswerHeader.len){
        break;
      }
      for(unsigned char i = 0; i < AnswerHeader.len; i++){  //copy all body
        bt = XIAOMI_PORT.read();
        *bufPtr++ = bt; 
        readCounter++;
        if(i < (AnswerHeader.len - 2)){ //exclude crc in packet
          calcCs -= bt;
        }
       
      }
      unsigned int * ipcs;
      ipcs = (unsigned int*)(bufPtr - 2);
      if(*ipcs == calcCs){   //check cs
        processPacket((unsigned char *) &buf, readCounter);
      }
      step = 0;
      break;
  }
}
void processPacket(unsigned char * data, unsigned char len){
  unsigned char RawDataLen;
  RawDataLen = len - sizeof(AnswerHeader) - 2;//(crc)
  switch(AnswerHeader.addr){ //store data into each other structure
    case 0x20: //0x20
      switch(AnswerHeader.cmd){
        case 0x00:
          switch(AnswerHeader.hz){
            case 0x64:
              if(RawDataLen == sizeof(A20C00HZ64)){
                memcpy((void*)& S20C00HZ64, (void*)data, RawDataLen);
                D.th = S20C00HZ64.throttle;
                D.br = S20C00HZ64.brake;
                _NewBrThData = 1;
              }
              break;
            case 0x65:
              if(Query.prepared == 1 && _Hibernate == 0){
                writeQuery();
              }
              memcpy((void*)& S20C00HZ65, (void*)data, RawDataLen);
              D.th = S20C00HZ65.throttle;
              D.br = S20C00HZ65.brake;
              _NewBrThData = 1;
              break;
            default: //no suitable hz
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
          break;
      }
      break;
    case 0x21:
      switch(AnswerHeader.cmd){
        case 0x00:
        switch(AnswerHeader.hz){
          case 0x64: //controller answer to BLE
            memcpy((void*)& S21C00HZ64, (void*)data, RawDataLen);
            if(S21C00HZ64.state == 1 || S21C00HZ64.state == 3){
              D.drive = 1;
            }else{
              D.drive = 0;
            }
            break;
          }
          break;
      default:
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
          break;
      }
      break;
    case 0x23:
      switch(AnswerHeader.cmd){
        case 0x17:
          break;
        case 0x1A:
          //if(RawDataLen == sizeof(A23C1A)){
          //  memcpy((void*)& S23C1A, (void*)data, RawDataLen);
          //}
          break;
        case 0x69:
          //if(RawDataLen == sizeof(A23C69)){
          //  memcpy((void*)& S23C69, (void*)data, RawDataLen);
          //}
          break;
        case 0x3E: //mainframe temperature
          //if(RawDataLen == sizeof(A23C3E)){
          //  memcpy((void*)& S23C3E, (void*)data, RawDataLen);
          //}
          break;
        case 0xB0: //speed, average speed, mileage total, mileage current, power on time, mainframe temp
          if(RawDataLen == sizeof(A23CB0)){
            memcpy((void*)& S23CB0, (void*)data, RawDataLen);
          }
          break;
        case 0x23: //remain mileage
          if(RawDataLen == sizeof(A23C23)){
            memcpy((void*)& S23C23, (void*)data, RawDataLen);
          }
          break;
        case 0x3A: //power on time, riding time
          if(RawDataLen == sizeof(A23C3A)){
            memcpy((void*)& S23C3A, (void*)data, RawDataLen);
          }
          break;
        case 0x7C:
          //if(RawDataLen == sizeof(A23C7C)){
            //memcpy((void*)& S23C7C, (void*)data, RawDataLen);
          //}
          break;
        case 0x7B:
          //if(RawDataLen == sizeof(A23C7B)){
            //memcpy((void*)& S23C7B, (void*)data, RawDataLen);
          //}
          break;
        case 0x7D:
          //if(RawDataLen == sizeof(A23C7D)){
            //memcpy((void*)& S23C7D, (void*)data, RawDataLen);
          //}
          break;
        default:
          break;
      }
      break;          
    case 0x25:
      switch(AnswerHeader.cmd){
        case 0x40: //cells info
          if(RawDataLen == sizeof(A25C40)){
            memcpy((void*)& S25C40, (void*)data, RawDataLen);
          }
          break;
        case 0x3B:
          //if(RawDataLen == sizeof(A25C3B)){
          //  memcpy((void*)& S25C3B, (void*)data, RawDataLen);
          //}
          break;
        case 0x31: //capacity, remain persent, current, voltage
          if(RawDataLen == sizeof(A25C31)){
            memcpy((void*)& S25C31, (void*)data, RawDataLen);
            if(D.initialPercent == 0){
              D.initialPercent  = S25C31.remainPercent;
              D.initialCapacity = S25C31.remainCapacity;
            }else{
              D.spentPercent  = D.initialPercent  - S25C31.remainPercent; //calculate spent energy
              D.spentCapacity = D.initialCapacity - S25C31.remainCapacity;
            }
          }
          break;
        case 0x20:
          //if(RawDataLen == sizeof(A25C20)){
          //  memcpy((void*)& S25C20, (void*)data, RawDataLen);
          //}
          break;
        case 0x1B:
          //_Hibernate = 1;
          //if(RawDataLen == sizeof(A25C1B)){
          //  memcpy((void*)& S25C1B, (void*)data, RawDataLen);
          //}
          break;
        case 0x10:
          //if(RawDataLen == sizeof(A25C10)){
          //  memcpy((void*)& S25C10, (void*)data, RawDataLen);
          //}
          break;
        default:
          break;
        }
        break;
      default:
        break;
  }

  for(unsigned char i = 0; i < sizeof(_commandsWeWillSend); i++){
    if(AnswerHeader.cmd == _q[_commandsWeWillSend[i]]){
      _NewDataFlag = 1;
      break;
    }
  }
}

void prepareNextQuery(){
  static unsigned char index = 0;
  switch(Menu.dispVar){
    case DV_SCR_5:
      Query.dynQueries[0] = 14; //CELLS
      Query.dynSize = 1;
      break;
    default:
      Query.dynQueries[0] =  1; //OTHER
      Query.dynQueries[1] =  8;
      Query.dynQueries[2] = 10;
      Query.dynSize = 3;
      break;
  }

  if(preloadQueryFromTable(Query.dynQueries[index]) == 0){
    Query.prepared = 1;
  }

  index++;
  if(index >= Query.dynSize){
    index = 0;
  }
}
unsigned char preloadQueryFromTable(unsigned char index){
  unsigned char * ptrBuf;
  unsigned char * pp; //pointer preamble
  unsigned char * ph; //pointer header
  unsigned char * pe; //pointer end

  unsigned char cmdFormat;
  unsigned char hLen; //header length
  unsigned char eLen; //ender length

  if(index >= sizeof(_q)){  //unknown index
    return 1;
  }

  if(Query.prepared != 0){ //if query not send yet
    return 2;
  }

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
    case 2: //h2 + end20t
      ph = (unsigned char*)&_h2;
      hLen = sizeof(_h2);

      //insert last known throttle & brake values
      _end20t.hz = 0x02;
      _end20t.th = D.th;
      _end20t.br = D.br;
      pe = (unsigned char*)&_end20t;
      eLen = sizeof(_end20t);
      break;
  }

  ptrBuf = (unsigned char*)&Query.buf;

  memcpy_P((void*)ptrBuf, (void*)pp, sizeof(_h0));  //copy preamble
  ptrBuf += sizeof(_h0);

  memcpy_P((void*)ptrBuf, (void*)ph, hLen);         //copy header
  ptrBuf += hLen;
  
  memcpy_P((void*)ptrBuf, (void*)(_q + index), 1);  //copy query
  ptrBuf++;
  
  memcpy_P((void*)ptrBuf, (void*)(_l + index), 1);  //copy expected answer length
  ptrBuf++;
  if(pe != NULL){
    memcpy((void*)ptrBuf, (void*)pe, eLen);       //if needed - copy ender
    ptrBuf+= hLen;
  }

  //unsigned char 
  Query.DataLen = ptrBuf - (unsigned char*)&Query.buf[2]; //calculate length of data in buf, w\o preamble and cs

  Query.cs = calcCs((unsigned char*)&Query.buf[2], Query.DataLen);    //calculate cs of buffer
  return 0;
}
void prepareCommand(unsigned char cmd){
  unsigned char * ptrBuf;

  _cmd.len  =    4;
  _cmd.addr = 0x20;
  _cmd.rlen = 0x03;

  switch(cmd){
    case CMD_CRUISE_ON:   //0x7C, 0x01, 0x00
      _cmd.param = 0x7C;
      _cmd.value =    1;
      break;
    case CMD_CRUISE_OFF:  //0x7C, 0x00, 0x00
      _cmd.param = 0x7C;
      _cmd.value =    0;
      break;
    case CMD_LED_ON:      //0x7D, 0x02, 0x00
      _cmd.param = 0x7D;  
      _cmd.value =    2;
      break;
    case CMD_LED_OFF:     //0x7D, 0x00, 0x00
      _cmd.param = 0x7D;
      _cmd.value =    0;
      break;
    case CMD_WEAK:        //0x7B, 0x00, 0x00
      _cmd.param = 0x7B;
      _cmd.value =    0;
      break;
    case CMD_MEDIUM:      //0x7B, 0x01, 0x00
      _cmd.param = 0x7B;
      _cmd.value =    1;
      break;
    case CMD_STRONG:      //0x7B, 0x02, 0x00
      _cmd.param = 0x7B;
      _cmd.value =    2;
      break;
    default:
      return; //undefined command - do nothing
      break;
  }
  ptrBuf = (unsigned char*)&Query.buf;

  memcpy_P((void*)ptrBuf, (void*)_h0, sizeof(_h0));  //copy preamble
  ptrBuf += sizeof(_h0);

  memcpy((void*)ptrBuf, (void*)&_cmd, sizeof(_cmd)); //copy command body
  ptrBuf += sizeof(_cmd);

  //unsigned char 
  Query.DataLen = ptrBuf - (unsigned char*)&Query.buf[2];               //calculate length of data in buf, w\o preamble and cs
  Query.cs = calcCs((unsigned char*)&Query.buf[2], Query.DataLen);     //calculate cs of buffer

  Query.commandLoad = 1;
  Query.prepared = 1;
}

void writeQuery(){
  RX_DISABLE;
  XIAOMI_PORT.write((unsigned char*)&Query.buf, Query.DataLen + 2);     //DataLen + length of preamble
  XIAOMI_PORT.write((unsigned char*)&Query.cs, 2);
  XIAOMI_PORT.flush(); //wait while data transfer in progress
  RX_ENABLE;
  Query.commandLoad = 0;
  Query.prepared = 0;
}

unsigned int calcCs(unsigned char * data, unsigned char len){
  unsigned int cs = 0xFFFF;
  for(int i = len; i > 0; i--){
    cs -= *data++;
  }
  return cs;
}

void flushRxBuffer(){
  while(XIAOMI_PORT.available()){
    XIAOMI_PORT.read();
  }
}


/* ODO 1
 * ODO 2
 * 
 * curr mileage
 * spent percent
 * power on time
 * 
 * Remain battery
 * 7800mAh 90%
 * remain mileage
 *
 * total mile 
 * mainframe temp
 * t1   t2
 * 
 */


//printMenu(_Menu.selItem, 3, menuMainItems);
void printBig(int p, const char * cs){ //TRIP
  int ph = abs(p) / 100;
  int pl = abs(p) % 100;
  char out[10];
  char s[8];

  strcpy_P(s, cs);
  
  //for cell drawing
  int chy, chg; //6 = bottom
  int charge = 50 - D.remPercent / 2;
  charge = constrain(charge, 0, 50);
  chy = charge + 6;
  chg = 50 - charge;



  u8g2.firstPage();
  do {
    //u8g2.drawBox(0,28, 16, 8);  //MINUS
    if(p < 0){   //ivert colors if value is negative
      u8g2.setDrawColor(1);    
      u8g2.drawBox(0,0, 128, 64); 
      u8g2.setDrawColor(0);     
    }else{
      u8g2.setDrawColor(0);
      u8g2.drawBox(0,0, 128, 64);
      u8g2.setDrawColor(1);
    }
    
    u8g2.setFont(u8g2_font_logisoso54_tn); //big
    if(ph < 10){
      u8g2.drawStr(34, 59, ultoa(ph, (char*)&out, 10));  
    }else{
      u8g2.drawStr(0, 59, ultoa(ph, (char*)&out, 10));
    }

    u8g2.setFont(u8g2_font_logisoso22_tr); //small
    if(pl < 10){
      u8g2.drawStr(70, 59, "0");
      u8g2.drawStr(85, 59, ultoa(pl, (char*)&out, 10));
    }else{
      u8g2.drawStr(70, 59, ultoa(pl, (char*)&out, 10));
    }

    u8g2.setFont(u8g2_font_ncenB10_tr);
    u8g2.drawStr(70, 14, s);    //text

    //cell
    u8g2.drawBox  (117, 0,  6,  4);     //nipple
    u8g2.drawFrame(113, 4, 14, 54);     //outline
    u8g2.drawBox  (115, chy, 10, chg);  //charge

  } while (u8g2.nextPage());
}



void print_display_1(){

Ls.title = (char*)t1;
Ls.p1 = (char*)s11;
printStallScreen(&Ls);

}

void printStallScreen(LS * ls){
//const char * spentStr = "SPENT";
//char * titlePtr = (char*)spentStr;

  //char buf[64];

  //char out[10];
  u8g2.setFont(u8g2_font_ncenB10_tr);   
//mileage | spent % | 
  D.spentPercent = 99;
  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1);
    //u8g2.drawStr((u8g2.getDisplayWidth() / 2 - u8g2.getStrWidth(ls->title) / 2), u8g2.getMaxCharHeight() - 2, ls->title); //print title in center

    u8g2.drawStr((u8g2.getDisplayWidth() / 2 - u8g2.getStrWidth(ls->title) / 2), u8g2.getMaxCharHeight() - 2, ls->title); //print title in center

    //u8g2.drawStr(1, 26, ls->p1);
    //u8g2.drawStr(u8g2.getStrWidth(ls->p1) + 4, 26, ultoa(D.spentPercent, (char*)&out, 10));

    u8g2.drawStr(1, 30, "125:54   Av 22.2"); //26
    u8g2.drawStr(1, 60, "Av12"); //26
    u8g2.drawStr(1, 45, "12800mA       89%"); //26
    

    
    //u8g2.drawStr(1, 26, ultoa(S21C00HZ64.state, (char*)&out, 10));
    //u8g2.drawStr(40, 26, "%");
    
    //u8g2.drawStr(52, 26, ultoa(S20C00HZ65.throttle, (char*)&out, 10));

    //u8g2.drawStr(1, 54, ultoa(loopsCounter, (char*)&out, 10));

    //u8g2.drawStr(52, 54, ultoa(S20C00HZ64.throttle, (char*)&out, 10));

  } while (u8g2.nextPage());
}


void print_display_11(){

#ifdef SCR_TESTING
  //printBig(1234, dispBp[1]);
  //return;
  #endif

  char out[10];
  u8g2.setFont(u8g2_font_ncenB10_tr);   
//mileage | spent % | 
  
  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1);
    u8g2.drawStr(1, 11, "Trip");
    
    u8g2.drawStr(1, 26, ultoa(S21C00HZ64.state, (char*)&out, 10));
    u8g2.drawStr(52, 26, ultoa(S20C00HZ65.throttle, (char*)&out, 10));
    u8g2.drawStr(1, 54, ultoa(loopsCounter, (char*)&out, 10));
    u8g2.drawStr(52, 54, ultoa(S20C00HZ64.throttle, (char*)&out, 10));

  } while (u8g2.nextPage());
}

void printMenu(int selItem, const char * Items){ //selected item, string with items
  char s[100];
  char * pch;
  char * strs[10]; //pointers to each item from source string
  int    itemsCounter = 0;

  selItem --;

  memset((void*)strs, (int)NULL, sizeof(strs));
  strcpy_P(s, Items);                
  pch = strtok(s, "\n");      //fill array with pointers to items
  while(pch != NULL){       
    strs[itemsCounter] = pch;
    pch = strtok(NULL, "\n");
    itemsCounter++;
    if(itemsCounter >= (int)sizeof(strs)){
      break;
    }
  }

  int selBoxOff = 1;
  int strXOff   = 3;

  int selBoxH;
  int selBoxW;
  int strH;     //string heigh in px
  int selStrW;  //width of selected string

  u8g2.setFont(u8g2_font_ncenB12_tr);   
  strH    = u8g2.getAscent();
  selStrW = u8g2.getStrWidth(strs[selItem]) + 2; //calculate selection box width

  selBoxW = selStrW;
  selBoxH = strH + 2; //+offset

  int upSt = 0;
  if(selItem > 3){
    upSt = selItem - 3;
  }

  u8g2.firstPage();
  do {
    u8g2.setDrawColor(1); //                 \/ first line offset
    u8g2.drawBox(0,(selBoxH + selBoxOff) * (selItem - upSt), selBoxW, selBoxH);  //draw white selection box


    //draw strings
    for(int i = 0; i < 4; i++){
      if(i == (selItem - upSt)){
        u8g2.setDrawColor(0); //selected string (black)
      }else{
        u8g2.setDrawColor(1); //another string (white)
      }
      if(strs[i] == NULL){
        break;
      }
      u8g2.drawStr(1, strH + 1 + ((strXOff + strH) * i), strs[upSt + i]); //draw string with counting offset

      
    }
  } while (u8g2.nextPage());
}


/*

screenQuantity   [4]
allItemsNum      [2]
selectorPosition [1-4]

//строки начинаются с 1

allItemsNum > screenQuantity && selectorPosition

if(selector == 1){ //первая строка
}

if(selector == allItemsNum){ //последня строка
}

//промежуточные варианты
 
 
 */


void print_display_2(){ //TRIP
  print_display_1();
}
void print_display_3(){ //MILEAGE
/*
Trip
16Km 12:16

Spent
28% 2860mA
*/
  print_display_1();
}
void print_display_31(){ //BATT
  print_display_1();
}
void print_display_4(){  //ODO
  print_display_1();
}
void print_display_5(){ //CELLS
  print_display_1();
/*
//battery voltage      
      display.setCursor(0, 48);
      v = D.voltage / 100;          //print battery voltage
      display.print  (F("U:")); 
      display.print  (v);
      display.print  ('.');
      v = D.voltage % 100;
      if(v < 10){
        display.print('0');
      }
      display.println(v);

//battery temperature
      display.setCursor(60, 48);
      display.print(F("t1:"));
      display.print((int)D.temp1);      

      display.setCursor(60, 56);
      display.print(F("t2:"));
      display.print((int)D.temp2);
*/
}

void serialEvent(){
//  if(Serial.available() > 0){
//    m365.processData();
//  }
}


/*
 * batt
 * design capacity
 * charging cycles
 * 
 * 
 * 
*/
