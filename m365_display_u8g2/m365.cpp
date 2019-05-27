#include "m365.h"

M365::M365(HardwareSerial * hp){
   port = hp;
}

void M365::processData(){
  
}


void M365::writeQuery(){
  
}

void M365::processPacket(unsigned char * data, unsigned char len){
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
                th = S20C00HZ64.throttle;
                br = S20C00HZ64.brake;
                NewBrThData = 1;
              }
              break;
            case 0x65:
              if(Query.prepared == 1 && Hibernate == 0){
                writeQuery();
              }
              memcpy((void*)& S20C00HZ65, (void*)data, RawDataLen);
              th = S20C00HZ65.throttle;
              br = S20C00HZ65.brake;
              NewBrThData = 1;
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
              drive = 1;
            }else{
              drive = 0;
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
            if(initialPercent == 0){
              initialPercent  = S25C31.remainPercent;
              initialCapacity = S25C31.remainCapacity;
            }else{
              spentPercent  = initialPercent  - S25C31.remainPercent; //calculate spent energy
              spentCapacity = initialCapacity - S25C31.remainCapacity;
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

/*
  for(unsigned char i = 0; i < sizeof(_commandsWeWillSend); i++){
    if(AnswerHeader.cmd == _q[_commandsWeWillSend[i]]){
      _NewDataFlag = 1;
      break;
    }
  }
  */
}
