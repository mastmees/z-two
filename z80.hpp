/* The MIT License (MIT)
 
  Copyright (c) 2018 Madis Kaal <mast@nomad.ee>
 
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:
 
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
 
  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#ifndef __z80_hpp_included__
#define __z80_hpp_included__

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#ifdef __AVR_ARCH__
#include <util/delay.h>
#endif
#include "z80_flags.h"
#include "aux.hpp"
#include "console.hpp"

#define noINSTRUCTIONDEBUG
#define noINSTRUCTIONPROFILER
#define INSTRUCTIONCOUNTER
#define noINCREMENTREFRESHREGISTER
#define USEREGISTERVARIABLES
#define PRINTINSTRUCTIONERRORS
// this macro would be defined if you'd have system with interrupt and/or NMI inputs
// if empty, no interrupt checking code is created
#define checkforinterrupts()

#ifdef __AVR_ARCH__
#define instructioncounter_t uint32_t
#else
#define instructioncounter_t uint64_t
#endif

#ifdef PRINTINSTRUCTIONERRORS
#define ERRORPRINT(a) console.print(a)
#define ERRORPHEX(a) console.phex(a)
#define ERRORPHEX16(a) console.phex16(a)
#define ERRORSEND(a) console.send(a)
#else
#define ERRORPRINT(a)
#define ERRORPHEX(a)
#define ERRORPHEX16(a)
#define ERRORSEND(a)
#endif

#ifdef INSTRUCTIONDEBUG
extern bool instdebbuging;
void DEBUGPRINT(const char*s);
void DEBUGPHEX(uint8_t b);
void DEBUGPHEX16(uint16_t w);
void DEBUGSEND(uint8_t c);
uint8_t DEBUGKEY(void);
#else
  #define DEBUGPRINT(a)
  #define DEBUGPHEX(a)
  #define DEBUGPHEX16(a)
  #define DEBUGSEND(a)
  #define DEBUGKEY()
#endif

#ifdef INSTRUCTIONPROFILER
extern instructioncounter_t profilercounts[256];
#ifndef INSTRUCTIONCOUNTER
#define INSTRUCTIONCOUNTER
#endif
#endif

#ifdef INSTRUCTIONCOUNTER
extern instructioncounter_t profilecounter;
#endif

#define swap(tmp,a,b) tmp=a;a=b;b=tmp

#ifndef __AVR_ARCH__
#undef USEREGISTERVARIABLES
#endif

typedef union 
{
  struct {
    uint8_t low;
    uint8_t high;
  } bytes;
  uint16_t word;
} register16;

// use registers directly for most used registers
// this gives approx 10% IPS throughput boost
//
#ifdef USEREGISTERVARIABLES
register uint8_t acc asm("r2");
register uint8_t flags asm("r3");
register uint16_t pcreg asm("r4");
register uint16_t spreg asm("r6");
register uint16_t tempw asm("r8");
register uint8_t tempb asm("r10");
#endif

class z80
{
  #ifndef USEREGISTERVARIABLES
  uint8_t acc;
  uint8_t flags;
  uint16_t pcreg;
  uint16_t spreg;
  uint16_t tempw;
  uint8_t tempb;
  #endif
  uint8_t acc2,flags2;
  register16 bc,bc2;
  register16 de,de2;
  register16 hl,hl2;
  register16 ir;
  register16 ix;
  register16 iy;
  bool halted;
  bool iff1,iff2;
  uint8_t im;

  inline uint8_t fetch() {
    #ifdef INSTRUCTIONDEBUG
    uint8_t b=readram(pcreg++);
    DEBUGPHEX(b);
    DEBUGSEND(' ');
    return b;
    #else
    return readram(pcreg++);
    #endif
  }
  
  inline uint16_t fetchw() { return ((uint16_t)fetch())|((uint16_t)(fetch())<<8); }
  inline void pushb(uint8_t b) { writeram(--spreg,b); }
  inline uint8_t popb() { return readram(spreg++); }
  inline void pushw(uint16_t w) 
  { 
    writeram(--spreg,w>>8);
    writeram(--spreg,w&0xff); 
  }
  inline uint16_t popw() 
  { 
    return popb()|((uint16_t)(popb())<<8); 
  }

  // these emulate instructions on extended instruction pages
  // and are called by emulate() as needed
  void emulate_cb();
  bool emulate_dd();
  void emulate_ddcb();
  void emulate_ed();
  bool emulate_fd();
  void emulate_fdcb();
  
  // these to substraction and addition and also set all flags as required
  void daa();
  uint8_t inc8(uint8_t a);
  uint8_t dec8(uint8_t a);
  uint8_t add8(uint8_t a,uint8_t b);
  uint8_t sub8(uint8_t a,uint8_t b);
  uint8_t adc8(uint8_t a,uint8_t b);
  uint8_t sbc8(uint8_t a,uint8_t b);
  
  uint16_t add16(uint16_t a,uint16_t b);
  uint16_t adc16(uint16_t a,uint16_t b);
  uint16_t sbc16(uint16_t a,uint16_t b);

  // set Z, PV, and S flags based on value r
  #define setlogicflags(r) flags=( (flags& (~(ZFLAG|PVFLAG|SFLAG))) | z80_logicflags[(uint8_t)r] )
  
public:

  void reset()
  {
    pcreg=0x0000;
    spreg=0xffff;
    acc=0xff;
    flags=0xff;
    halted=false;
    im=0;
    iff1=false;
    iff2=false;
  }
  
  void step(uint16_t count=2048);

/*
implement these somewhere for your hardware
*/
  uint8_t readram(uint16_t adr);
  void writeram(uint16_t adr,uint8_t data);
  uint8_t readio(uint16_t adr);
  void writeio(uint16_t adr,uint8_t data);
  void fault(void);   
    
};

#endif
