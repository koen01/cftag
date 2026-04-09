#ifndef __AES_H__
#define __AES_H__

#if defined(AVR)
#include <avr/pgmspace.h>
#else
#include <pgmspace.h>
#endif

typedef unsigned char byte;

class AES
{
public:
  int encrypt(int keytype, byte plain[16], byte cipher[16]);
private:
  byte set_key(int keytype);
  byte key_sched[240];
};

#endif
