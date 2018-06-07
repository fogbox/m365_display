#ifndef MESSAGES_h
#define MESSAGES_h

#define MAX_MESSAGES  30
#define MAX_BROADCAST 10

#include "Arduino.h"

#define DROP  0
#define NEW   1
#define READY 2


class MessagesClass
{
  private:
    unsigned char messages[MAX_MESSAGES];
    unsigned char broadcast[MAX_BROADCAST];

  public:
    MessagesClass();
    void Post(unsigned char);
    unsigned char Get(unsigned char);
    unsigned char Peek(unsigned char); //for sniffer
    void Process();
    void PostBroadcast(unsigned char);
    unsigned char GetBroadcast(unsigned char);
    void ProcessBroadcast();
};

#endif
