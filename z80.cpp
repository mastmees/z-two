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
#include "z80.hpp"
#include <string.h>

#ifdef INSTRUCTIONDEBUG
static const char *rnames[] = { "b","c","d","e","h","l","(hl)","a" };
bool instdebugging = false;

void DEBUGPRINT(const char*s)
{
  if (instdebugging) 
    aux.print(s); 
}

void DEBUGPHEX(uint8_t b)
{
  if (instdebugging)
    aux.phex(b); 
}

void DEBUGPHEX16(uint16_t w)
{
  if (instdebugging)
    aux.phex16(w);
}

void DEBUGSEND(uint8_t c)
{
  if (instdebugging) 
    aux.send(c);
}

uint8_t  DEBUGKEY(void)
{
  if (aux.rxready())
    return aux.receive();
  return 0;
}
#endif

#ifdef INSTRUCTIONPROFILER
// basic instruction execution counts
instructioncounter_t profilercounts[256];
#endif

#ifdef INSTRUCTIONCOUNTER
instructioncounter_t profilecounter;
#endif

void z80::daa()
{
int16_t iacc;
uint8_t y;
  iacc=acc;
  tempb=acc&0x0f;
  if (testflag(NFLAG)) { // post substraction
    y=carryflag() || iacc>0x99;
    if (testflag(HFLAG) || (tempb>9)) {
      if (tempb>5)
        clearflags(HFLAG);
      iacc-=6;
      iacc&=0xff;
    }
    if (y)
      iacc-=0x160;
  }
  else {// post addition
    if (testflag(HFLAG) || (tempb>9)) {
       if (tempb>9)
         setflags(HFLAG);
       else
         clearflags(HFLAG);
       iacc+=6;
    }
    if (carryflag() || (iacc&0x1f0)>0x90)
      iacc+=0x60;
  }
  acc=iacc&255;
  setlogicflags(acc);
  if (iacc&0x100)
    setflags(CFLAG);
}

uint8_t z80::inc8(uint8_t a)
{
uint8_t b;
  b=a+1;
  setlogicflags(b);
  clearflags(NFLAG|PVFLAG|HFLAG);
  if ((a&0x0f)==0x0f)
    setflags(HFLAG);
  if (a==0x7f)
    setflags(PVFLAG);
  return b;
}

uint8_t z80::dec8(uint8_t a)
{
uint8_t b;
  b=a-1;
  setlogicflags(b);
  setflags(NFLAG);
  clearflags(PVFLAG|HFLAG);
  if ((a&0x0f)==0)
    setflags(HFLAG);
  if (a==0x80)
    setflags(PVFLAG);
  return b;
}

// overflow has occured if two numbers with same sign are added together
// and the sign changes. overflow never occurs if numbers with different signs are
// added together

uint8_t z80::add8(uint8_t a,uint8_t b) // add and set flags
{
uint16_t s;
uint16_t x;
  s=(uint16_t)a+(uint16_t)b;
  x=a^b^s;
  setlogicflags((uint8_t)s); // S and Z
  clearflags(NFLAG|CFLAG|HFLAG|PVFLAG);
  if (s&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint8_t)s;
}

uint8_t z80::adc8(uint8_t a,uint8_t b) // add and set flags
{
uint16_t s;
uint16_t x;
  s=(uint16_t)a+(uint16_t)b+carryflag();
  x=a^b^s;
  setlogicflags((uint8_t)s); // S and Z
  clearflags(NFLAG|CFLAG|HFLAG|PVFLAG);
  if (s&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint8_t)s;
}

uint16_t z80::add16(uint16_t a,uint16_t b) // add and set flags
{
uint32_t s;
uint16_t x;
  s=(uint32_t)a+(uint32_t)b;
  x=(a^b^s)>>8;
  clearflags(NFLAG|HFLAG|CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if (x&0x100)
    setflags(CFLAG);  
  return (uint16_t)s;
}

uint16_t z80::adc16(uint16_t a,uint16_t b) // add and set flags
{
uint32_t s;
uint32_t x;
  s=(uint32_t)a+(uint32_t)b+carryflag();
  x=(a^b^s)>>8;
  clearflags(NFLAG|HFLAG|CFLAG|ZFLAG|PVFLAG|SFLAG);
  if (x&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if (!(uint16_t)s)
    setflags(ZFLAG);
  if (s&0x8000)
    setflags(SFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint16_t)s;
}

uint8_t z80::sub8(uint8_t a,uint8_t b) // substract and set flags
{
uint16_t s;
uint16_t x;
  s=(uint16_t)a-(uint16_t)b;
  x=(a^b^s);
  setlogicflags((uint8_t)s); // S & Z
  setflags(NFLAG);
  clearflags(CFLAG|HFLAG|PVFLAG);
  if (s&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint8_t)s;
}

uint8_t z80::sbc8(uint8_t a,uint8_t b)
{
uint16_t s;
uint16_t x;
  s=(uint16_t)a-(uint16_t)b-carryflag();
  x=(a^b^s);
  setlogicflags((uint8_t)s); // S & Z
  setflags(NFLAG);
  clearflags(CFLAG|HFLAG|PVFLAG);
  if (s&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint8_t)s;
}

uint16_t z80::sbc16(uint16_t a,uint16_t b) // substract and set flags
{
uint32_t s;
uint32_t x;
  s=(uint32_t)a-(uint32_t)b-carryflag();
  x=(a^b^s)>>8;
  clearflags(HFLAG|CFLAG|ZFLAG|PVFLAG|SFLAG);
  setflags(NFLAG);
  if (x&0x100)
    setflags(CFLAG);
  if (x&0x10)
    setflags(HFLAG);
  if (!(uint16_t)s)
    setflags(ZFLAG);
  if (s&0x8000)
    setflags(SFLAG);
  if ((x^(x>>1))&0x80)
    setflags(PVFLAG);
  return (uint16_t)s;
}

void z80::step(uint16_t count)
{
#ifdef INSTRUCTIONDEBUG
uint8_t x;
const char flagnames[8]={'S','Z','_','H','_','P','N','C'};
#endif
  while (count--) {
    #ifdef INSTRUCTIONDEBUG
    DEBUGPHEX(pcreg>>8);
    DEBUGPHEX(pcreg);
    DEBUGSEND(':');
    #endif
    tempb=fetch();
    #ifdef INSTRUCTIONPROFILER
    profilercounts[tempb]++;
    #endif
    #ifdef INSTRUCTIONCOUNTER
    profilecounter++;
    #endif
    #ifdef INCREMENTREFRESHREGISTER
    ir.bytes.low++;
    #endif
// if unrecognized instruction is encountered after DD or FD prefix
// then the instruction is treated as unprefixed by coming back here
ignoreprefix:
    switch (tempb) {
      case 0x00: // nop
        DEBUGPRINT("         nop\t");
        break;
      case 0x01: // ld bc,xxxx
        bc.word=fetchw();
        DEBUGPRINT("   ld bc,");
        DEBUGPHEX16(bc.word);
        break;
      case 0x02: // ld (bc),a
        writeram(bc.word,acc);
        DEBUGPRINT("         ld (bc),a");
        break;
      case 0x03: // inc bc
        bc.word++;
        DEBUGPRINT("         inc bc\t");
        break;
      case 0x04: // inc b
        bc.bytes.high=inc8(bc.bytes.high);
        DEBUGPRINT("         inc b\t");
        break;
      case 0x05: // dec b
        bc.bytes.high=dec8(bc.bytes.high);
        DEBUGPRINT("         dec b\t");
        break;
      case 0x06: // ld b,xx
        bc.bytes.high=fetch();
        DEBUGPRINT("      ld b,");
        DEBUGPHEX(bc.bytes.high);
        break;
      case 0x07: // rlca
        tempb=acc;
        acc=(tempb<<1)|((tempb&0x80)?1:0);
        if (tempb&0x80)
          setflags(CFLAG);
        else
          clearflags(CFLAG);
        clearflags(HFLAG|NFLAG);
        DEBUGPRINT("         rlca\t");
        break;
      case 0x08: // ex af,af'
        swap(tempb,acc,acc2);
        swap(tempb,flags,flags2);
        DEBUGPRINT("         ex af,af'");
        break;
      case 0x09: // add hl,bc
        hl.word=add16(hl.word,bc.word);
        DEBUGPRINT("         add hl,bc");
        break;
      case 0x0a: // ld a,(bc)
        acc=readram(bc.word);
        DEBUGPRINT("         ld a,(bc)");
        break;
      case 0x0b: // dec bc
        bc.word--;
        DEBUGPRINT("         dec bc\t");
        break;
      case 0x0c: // inc c
        bc.bytes.low=inc8(bc.bytes.low);
        DEBUGPRINT("         inc c\t");
        break;
      case 0x0d: // dec c
        bc.bytes.low=dec8(bc.bytes.low);
        DEBUGPRINT("         dec c\t");
        break;
      case 0x0e: // ld c,xx
        bc.bytes.low=fetch();
        DEBUGPRINT("      ld c,");
        DEBUGPHEX(bc.bytes.low);
        break;
      case 0x0f: // rrca
        tempb=acc;
        acc=(tempb>>1)|((tempb&1)?0x80:0);
        if (tempb&1)
          setflags(CFLAG);
        else
          clearflags(CFLAG);
        clearflags(HFLAG|NFLAG);
        DEBUGPRINT("         rrca\t");
        break;
      case 0x10: // djnz xx
        tempw=(int8_t)fetch()+pcreg;
        bc.bytes.high--;
        if (bc.bytes.high)
          pcreg=tempw;
        DEBUGPRINT("      djnz ");
        DEBUGPHEX16(tempw);
        break;
      case 0x11: // ld de,xxxx
        de.word=fetchw();
        DEBUGPRINT("   ld de,");
        DEBUGPHEX16(de.word);
        break;
      case 0x12: // ld (de),a
        writeram(de.word,acc);
        DEBUGPRINT("         ld (de),a");
        break;
      case 0x13: // inc de
        de.word++;
        DEBUGPRINT("         inc de\t");
        break;
      case 0x14: // inc d
        de.bytes.high=inc8(de.bytes.high);
        DEBUGPRINT("         inc d\t");
        break;
      case 0x15: // dec d
        de.bytes.high=dec8(de.bytes.high);
        DEBUGPRINT("         dec d\t");
        break;
      case 0x16: // ld d,xx
        de.bytes.high=fetch();
        DEBUGPRINT("      ld d,");
        DEBUGPHEX(de.bytes.high);
        break;
      case 0x17: // rla
        tempb=acc;
        acc=(tempb<<1)|carryflag();
        if (tempb&0x80)
          setflags(CFLAG);
        else
          clearflags(CFLAG);
        clearflags(HFLAG|NFLAG);
        DEBUGPRINT("         rla\t");
        break;
      case 0x18: // jr xx
        pcreg=(int8_t)fetch()+pcreg;
        DEBUGPRINT("      jr ");
        DEBUGPHEX16(pcreg);
        break;
      case 0x19: // add hl,de
        hl.word=add16(hl.word,de.word);
        DEBUGPRINT("         add hl,de");
        break;
      case 0x1a: // ld a,(de)
        acc=readram(de.word);
        DEBUGPRINT("         ld a,(de)");
        break;
      case 0x1b: // dec de
        de.word--;
        DEBUGPRINT("         dec de\t");
        break;
      case 0x1c: // inc e
        de.bytes.low=inc8(de.bytes.low);
        DEBUGPRINT("         inc e\t");
        break;
      case 0x1d: // dec e
        de.bytes.low=dec8(de.bytes.low);
        DEBUGPRINT("         dec e\t");
        break;
      case 0x1e: // ld e,xx
        de.bytes.low=fetch();
        DEBUGPRINT("      ld e,");
        DEBUGPHEX(de.bytes.low);
        break;
      case 0x1f: // rra
        tempb=acc;
        acc=(tempb>>1)|(carryflag()?0x80:0);
        if (tempb&1)
          setflags(CFLAG);
        else
          clearflags(CFLAG);
        clearflags(HFLAG|NFLAG);
        DEBUGPRINT("         rra\t");
        break;
      case 0x20: // jr nz,xx
        tempw=(int8_t)fetch()+pcreg;
        if (!testflag(ZFLAG))
          pcreg=tempw;
        DEBUGPRINT("      jr nz,");
        DEBUGPHEX16(w);
        break;
      case 0x21: // ld hl,xxxx
        hl.word=fetchw();
        DEBUGPRINT("   ld hl,");
        DEBUGPHEX16(hl.word);
        break;
      case 0x22: // ld (xxxx),hl
        tempw=fetchw();
        writeram(tempw,hl.bytes.low);
        writeram(tempw+1,hl.bytes.high);
        DEBUGPRINT("   ld (");
        DEBUGPHEX16(tempw);
        DEBUGPRINT(",hl");
        break;
      case 0x23: // inc hl
        hl.word++;
        DEBUGPRINT("         inc hl\t");
        break;
      case 0x24: // inc h
        hl.bytes.high=inc8(hl.bytes.high);
        DEBUGPRINT("         inc h\t");
        break;
      case 0x25: // dec h
        hl.bytes.high=dec8(hl.bytes.high);
        DEBUGPRINT("         dec h\t");
        break;
      case 0x26: // ld h,xx
        hl.bytes.high=fetch();
        DEBUGPRINT("      ld h,");
        DEBUGPHEX(hl.bytes.high);
        break;
      case 0x27: // daa
                 // method stolen from https://github.com/mamedev/mame/blob/master/src/devices/cpu/z80/z80.cpp
        daa();
        DEBUGPRINT("         daa\t");
        break;
      case 0x28: // jr z,xx
        tempw=(int8_t)fetch()+pcreg;
        if (testflag(ZFLAG))
          pcreg=tempw;
        DEBUGPRINT("      jr z,");
        DEBUGPHEX16(tempw);
        break;
      case 0x29: // add hl,hl
        hl.word=add16(hl.word,hl.word);
        DEBUGPRINT("         add hl,hl");
        break;
      case 0x2a: // ld hl,(xxxx)
        tempw=fetchw();
        hl.bytes.low=readram(tempw);
        hl.bytes.high=readram(tempw+1);
        DEBUGPRINT("   ld hl,(");
        DEBUGPHEX16(tempw);
        DEBUGPRINT(")");
        break;
      case 0x2b: // dec hl
        hl.word--;
        DEBUGPRINT("         dec hl\t");
        break;
      case 0x2c: // inc l
        hl.bytes.low=inc8(hl.bytes.low);
        DEBUGPRINT("         inc l\t");
        break;
      case 0x2d: // dec l
        hl.bytes.low=dec8(hl.bytes.low);
        DEBUGPRINT("         dec l\t");
        break;
      case 0x2e: // ld l,xx
        hl.bytes.low=fetch();
        DEBUGPRINT("      ld l,");
        DEBUGPHEX(hl.bytes.low);
        break;
      case 0x2f: // cpl
        acc=~acc;
        setflags(NFLAG|HFLAG);
        DEBUGPRINT("         cpl\t");
        break;
      case 0x30: // 
        tempw=(int8_t)fetch()+pcreg;
        if (!testflag(CFLAG))
          pcreg=tempw;
        DEBUGPRINT("      jr nc,");
        DEBUGPHEX16(tempw);
        break;
      case 0x31: // ld sp,xxxx
        spreg=fetchw();
        DEBUGPRINT("   ld sp,");
        DEBUGPHEX16(spreg);
        break;
      case 0x32: // ld (xxxx),a
        tempw=fetchw();
        writeram(tempw,acc);
        DEBUGPRINT("   ld (");
        DEBUGPHEX16(tempw);
        DEBUGPRINT("),a");
        break;
      case 0x33: // inc sp
        spreg++;
        DEBUGPRINT("         inc sp\t");
        break;
      case 0x34: // inc (hl)
        tempb=readram(hl.word);
        tempb=inc8(tempb);
        writeram(hl.word,tempb);
        DEBUGPRINT("         inc (hl)");
        break;
      case 0x35: // dec (hl)
        tempb=readram(hl.word);
        tempb=dec8(tempb);
        writeram(hl.word,tempb);
        DEBUGPRINT("         dec (hl)");
        break;
      case 0x36: // ld (hl),xx
        tempb=fetch();
        writeram(hl.word,tempb);
        DEBUGPRINT("      ld (hl),");
        DEBUGPHEX(tempb);
        break;
      case 0x37: // scf
        setflags(CFLAG);
        clearflags(NFLAG|HFLAG);
        DEBUGPRINT("         scf\t");
        break;
      case 0x38: // jr c,xx
        tempw=(int8_t)fetch()+pcreg;
        if (testflag(CFLAG))
          pcreg=tempw;
        DEBUGPRINT("      jr c,");
        DEBUGPHEX16(tempw);
        break;
      case 0x39: // add hl,sp
        hl.word=add16(hl.word,spreg);
        DEBUGPRINT("         add hl,sp");
        break;
      case 0x3a: // ld a,(xxxx)
        tempw=fetchw();
        acc=readram(tempw);
        DEBUGPRINT("   ld a,(");
        DEBUGPHEX16(tempw);
        DEBUGPRINT(")");
        break;
      case 0x3b: // dec sp
        spreg--;
        DEBUGPRINT("         dec sp");
        break;
      case 0x3c: // inc a
        acc=inc8(acc);
        DEBUGPRINT("         inc a\t");
        break;
      case 0x3d: // dec a
        acc=dec8(acc);
        DEBUGPRINT("         dec a\t");
        break;
      case 0x3e: // ld a,xx
        acc=fetch();
        DEBUGPRINT("      ld a,");
        DEBUGPHEX(acc);
        break;
      case 0x3f: // ccf
        clearflags(HFLAG|NFLAG);
        if (carryflag())
          setflags(HFLAG);
        flipflags(CFLAG);
        DEBUGPRINT("         ccf\t");
        break;
      case 0x40: // ld b,b
        DEBUGPRINT("         ld b,b\t");
        break;
      case 0x41: // ld b,c
        bc.bytes.high=bc.bytes.low;
        DEBUGPRINT("         ld b,c\t");
        break;
      case 0x42: // ld b,d
        bc.bytes.high=de.bytes.high;
        DEBUGPRINT("         ld b,d\t");
        break;
      case 0x43: // ld b,e
        bc.bytes.high=de.bytes.low;
        DEBUGPRINT("         ld b,e\t");
        break;
      case 0x44: // ld b,h
        bc.bytes.high=hl.bytes.high;
        DEBUGPRINT("         ld b,h\t");
        break;
      case 0x45: // ld b,l
        bc.bytes.high=hl.bytes.low;
        DEBUGPRINT("         ld b,l\t");
        break;
      case 0x46: // ld b,(hl)
        bc.bytes.high=readram(hl.word);
        DEBUGPRINT("         ld b,(hl)");
        break;
      case 0x47: // ld b,a
        bc.bytes.high=acc;
        DEBUGPRINT("         ld b,a\t");
        break;
      case 0x48: // ld c,b
        bc.bytes.low=bc.bytes.high;
        DEBUGPRINT("         ld c,b\t");
        break;
      case 0x49: // ld c,c
        DEBUGPRINT("         ld c,c\t");
        break;
      case 0x4a: // ld c,d
        bc.bytes.low=de.bytes.high;
        DEBUGPRINT("         ld c,d\t");
        break;
      case 0x4b: // ld c,e
        bc.bytes.low=de.bytes.low;
        DEBUGPRINT("         ld c,e\t");
        break;
      case 0x4c: // ld c,h
        bc.bytes.low=hl.bytes.high;
        DEBUGPRINT("         ld c,h\t");
        break;
      case 0x4d: // ld c,l
        bc.bytes.low=hl.bytes.low;
        DEBUGPRINT("         ld c,l\t");
        break;
      case 0x4e: // ld c,(hl)
        bc.bytes.low=readram(hl.word);
        DEBUGPRINT("         ld c,(hl)");
        break;
      case 0x4f: // ld c,a
        bc.bytes.low=acc;
        DEBUGPRINT("         ld c,a\t");
        break;
      case 0x50: // ld d,b
        de.bytes.high=bc.bytes.high;
        DEBUGPRINT("         ld d,b\t");
        break;
      case 0x51: // ld d,c
        de.bytes.high=bc.bytes.low;
        DEBUGPRINT("         ld d,c\t");
        break;
      case 0x52: // ld d,d
        DEBUGPRINT("         ld d,d\t");
        break;
      case 0x53: // ld d,e
        de.bytes.high=de.bytes.low;
        DEBUGPRINT("         ld d,e\t");
        break;
      case 0x54: // ld d,h
        de.bytes.high=hl.bytes.high;
        DEBUGPRINT("         ld d,h\t");
        break;
      case 0x55: // ld d,l
        de.bytes.high=hl.bytes.low;
        DEBUGPRINT("         ld d,l\t");
        break;
      case 0x56: // ld d,(hl)
        de.bytes.high=readram(hl.word);
        DEBUGPRINT("         ld d,(hl)");
        break;
      case 0x57: // ld d,a
        de.bytes.high=acc;
        DEBUGPRINT("         ld d,a\t");
        break;
      case 0x58: // ld e,b
        de.bytes.low=bc.bytes.high;
        DEBUGPRINT("         ld e,b\t");
        break;
      case 0x59: // ld e,c
        de.bytes.low=bc.bytes.low;
        DEBUGPRINT("         ld e,c\t");
        break;
      case 0x5a: // ld e,d
        de.bytes.low=de.bytes.high;
        DEBUGPRINT("         ld e,d\t");
        break;
      case 0x5b: // ld e,e
        DEBUGPRINT("         ld e,e\t");
        break;
      case 0x5c: // ld e,h
        de.bytes.low=hl.bytes.high;
        DEBUGPRINT("         ld e,h\t");
        break;
      case 0x5d: // ld e,l
        de.bytes.low=hl.bytes.low;
        DEBUGPRINT("         ld e,l\t");
        break;
      case 0x5e: // ld e,(hl)
        de.bytes.low=readram(hl.word);
        DEBUGPRINT("         ld e,(hl)");
        break;
      case 0x5f: // ld e,a
        de.bytes.low=acc;
        DEBUGPRINT("         ld e,a\t");
        break;
      case 0x60: // ld h,b
        hl.bytes.high=bc.bytes.high;
        DEBUGPRINT("         ld h,b\t");
        break;
      case 0x61: // ld h,c
        hl.bytes.high=bc.bytes.low;
        DEBUGPRINT("         ld h,c\t");
        break;
      case 0x62: // ld h,d
        hl.bytes.high=de.bytes.high;
        DEBUGPRINT("         ld h,d\t");
        break;
      case 0x63: // ld h,e
        hl.bytes.high=de.bytes.low;
        DEBUGPRINT("         ld h,e\t");
        break;
      case 0x64: // ld h,h
        DEBUGPRINT("         ld h,h\t");
        break;
      case 0x65: // ld h,l
        hl.bytes.high=hl.bytes.low;
        DEBUGPRINT("         ld h,l\t");
        break;
      case 0x66: // ld h,(hl)
        hl.bytes.high=readram(hl.word);
        DEBUGPRINT("         ld h,(hl)");
        break;
      case 0x67: // ld h,a
        hl.bytes.high=acc;
        DEBUGPRINT("         ld h,a\t");
        break;
      case 0x68: // ld l,b
        hl.bytes.low=bc.bytes.high;
        DEBUGPRINT("         ld l,b\t");
        break;
      case 0x69: // ld l,c
        hl.bytes.low=bc.bytes.low;
        DEBUGPRINT("         ld l,c\t");
        break;
      case 0x6a: // ld l,d
        hl.bytes.low=de.bytes.high;
        DEBUGPRINT("         ld l,d\t");
        break;
      case 0x6b: // ld l,e
        hl.bytes.low=de.bytes.low;
        DEBUGPRINT("         ld l,e\t");
        break;
      case 0x6c: // ld l,h
        hl.bytes.low=hl.bytes.high;
        DEBUGPRINT("         ld l,h\t");
        break;
      case 0x6d: // ld l,l
        DEBUGPRINT("         ld l,l\t");
        break;
      case 0x6e: // ld l,(hl)
        hl.bytes.low=readram(hl.word);
        DEBUGPRINT("         ld l,(hl)");
        break;
      case 0x6f: // ld l,a
        hl.bytes.low=acc;
        DEBUGPRINT("         ld l,a\t");
        break;
      case 0x70: // ld (hl),b
        writeram(hl.word,bc.bytes.high);
        DEBUGPRINT("         ld (hl),b");
        break;
      case 0x71: // ld (hl),c
        writeram(hl.word,bc.bytes.low);
        DEBUGPRINT("         ld (hl),c");
        break;
      case 0x72: // ld (hl),d
        writeram(hl.word,de.bytes.high);
        DEBUGPRINT("         ld (hl),d");
        break;
      case 0x73: // ld (hl),e
        writeram(hl.word,de.bytes.low);
        DEBUGPRINT("         ld (hl),e");
        break;
      case 0x74: // ld (hl),h
        writeram(hl.word,hl.bytes.high);
        DEBUGPRINT("         ld (hl),h");
        break;
      case 0x75: // ld (hl),l
        writeram(hl.word,hl.bytes.low);
        DEBUGPRINT("         ld (hl),l");
        break;
      case 0x76: // halt
        halted=true;
        DEBUGPRINT("         halt\t");
        break;
      case 0x77: // ld (hl),a
        writeram(hl.word,acc);
        DEBUGPRINT("         ld (hl),a");
        break;
      case 0x78: // ld a,b
        acc=bc.bytes.high;
        DEBUGPRINT("         ld a,b\t");
        break;
      case 0x79: // ld a,c
        acc=bc.bytes.low;
        DEBUGPRINT("         ld a,c\t");
        break;
      case 0x7a: // ld a,d
        acc=de.bytes.high;
        DEBUGPRINT("         ld a,d\t");
        break;
      case 0x7b: // ld a,e
        acc=de.bytes.low;
        DEBUGPRINT("         ld a,e\t");
        break;
      case 0x7c: // ld a,h
        acc=hl.bytes.high;
        DEBUGPRINT("         ld a,h\t");
        break;
      case 0x7d: // ld a,l
        acc=hl.bytes.low;
        DEBUGPRINT("         ld a,l\t");
        break;
      case 0x7e: // ld a,(hl)
        acc=readram(hl.word);
        DEBUGPRINT("         ld a,(hl)");
        break;
      case 0x7f: // ld a,a
        DEBUGPRINT("         ld a,a\t");
        break;
      case 0x80: // add a,b
        acc=add8(acc,bc.bytes.high);
        DEBUGPRINT("         add a,b");
        break;
      case 0x81: // add a,c
        acc=add8(acc,bc.bytes.low);
        DEBUGPRINT("         add a,c");
        break;
      case 0x82: // add a,d
        acc=add8(acc,de.bytes.high);
        DEBUGPRINT("         add a,d");
        break;
      case 0x83: // add a,e
        acc=add8(acc,de.bytes.low);
        DEBUGPRINT("         add a,e");
        break;
      case 0x84: // add a,h
        acc=add8(acc,hl.bytes.high);
        DEBUGPRINT("         add a,h");
        break;
      case 0x85: // add a,l
        acc=add8(acc,hl.bytes.low);
        DEBUGPRINT("         add a,l");
        break;
      case 0x86: // add a,(hl)
        acc=add8(acc,readram(hl.word));
        DEBUGPRINT("         add a,(hl)");
        break;
      case 0x87: // add a,a
        acc=add8(acc,acc);
        DEBUGPRINT("         add a,a");
        break;
      case 0x88: // adc a,b
        acc=adc8(acc,bc.bytes.high);
        DEBUGPRINT("         adc a,b");
        break;
      case 0x89: // adc a,c
        acc=adc8(acc,bc.bytes.low);
        DEBUGPRINT("         adc a,c");
        break;
      case 0x8a: // adc a,d
        acc=adc8(acc,de.bytes.high);
        DEBUGPRINT("         adc a,d");
        break;
      case 0x8b: // adc a,e
        acc=adc8(acc,de.bytes.low);
        DEBUGPRINT("         adc a,e");
        break;
      case 0x8c: // adc a,h
        acc=adc8(acc,hl.bytes.high);
        DEBUGPRINT("         adc a,h");
        break;
      case 0x8d: // adc a,l
        acc=adc8(acc,hl.bytes.low);
        DEBUGPRINT("         adc a,l");
        break;
      case 0x8e: // adc a,(hl)
        acc=adc8(acc,readram(hl.word));
        DEBUGPRINT("         adc a,(hl)");
        break;
      case 0x8f: // adc a,a
        acc=adc8(acc,acc);
        DEBUGPRINT("         adc a,a");
        break;
      case 0x90: // sub b
        acc=sub8(acc,bc.bytes.high);
        DEBUGPRINT("         sub b\t");
        break;
      case 0x91: // sub c
        acc=sub8(acc,bc.bytes.low);
        DEBUGPRINT("         sub c\t");
        break;
      case 0x92: // sub d
        acc=sub8(acc,de.bytes.high);
        DEBUGPRINT("         sub d\t");
        break;
      case 0x93: // sub e
        acc=sub8(acc,de.bytes.low);
        DEBUGPRINT("         sub e\t");
        break;
      case 0x94: // sub h
        acc=sub8(acc,hl.bytes.high);
        DEBUGPRINT("         sub h\t");
        break;
      case 0x95: // sub l
        acc=sub8(acc,hl.bytes.low);
        DEBUGPRINT("         sub l\t");
        break;
      case 0x96: // sub (hl)
        acc=sub8(acc,readram(hl.word));
        DEBUGPRINT("         sub (hl)");
        break;
      case 0x97: // sub a,a
        acc=sub8(acc,acc);
        DEBUGPRINT("         sub a\t");
        break;
      case 0x98: // sbc a,b
        acc=sbc8(acc,bc.bytes.high);
        DEBUGPRINT("         sbc a,b");
        break;
      case 0x99: // sbc a,c
        acc=sbc8(acc,bc.bytes.low);
        DEBUGPRINT("         sbc a,c");
        break;
      case 0x9a: // sbc a,d
        acc=sbc8(acc,de.bytes.high);
        DEBUGPRINT("         sbc a,d");
        break;
      case 0x9b: // sbc a,e
        acc=sbc8(acc,de.bytes.low);
        DEBUGPRINT("         sbc a,e");
        break;
      case 0x9c: // sbc a,h
        acc=sbc8(acc,hl.bytes.high);
        DEBUGPRINT("         sbc a,h");
        break;
      case 0x9d: // sbc a,l
        acc=sbc8(acc,hl.bytes.low);
        DEBUGPRINT("         sbc a,l");
        break;
      case 0x9e: // sbc a,(hl)
        acc=sbc8(acc,readram(hl.word));
        DEBUGPRINT("         sbc a,(hl)");
        break;
      case 0x9f: // sbc a,a
        acc=sbc8(acc,acc);
        DEBUGPRINT("         sbc a,a");
        break;
      case 0xa0: // and b
        acc&=bc.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and b\t");
        break;
      case 0xa1: // and c
        acc&=bc.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and c\t");
        break;
      case 0xa2: // and d
        acc&=de.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and d\t");
        break;
      case 0xa3: // and e
        acc&=de.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and e\t");
        break;
      case 0xa4: // and h
        acc&=hl.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and h\t");
        break;
      case 0xa5: // and l
        acc&=hl.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and l\t");
        break;
      case 0xa6: // and (hl)
        acc&=readram(hl.word);
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and (hl)");
        break;
      case 0xa7: // and a
        acc&=acc;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("         and a\t");
        break;
      case 0xa8: // xor b
        acc^=bc.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor b\t");
        break;
      case 0xa9: // xor c
        acc^=bc.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor c\t");
        break;
      case 0xaa: // xor d
        acc^=de.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor d\t");
        break;
      case 0xab: // xor e
        acc^=de.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor e\t");
        break;
      case 0xac: // xor h
        acc^=hl.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor h\t");
        break;
      case 0xad: // xor l
        acc^=hl.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor l\t");
        break;
      case 0xae: // xor (hl)
        acc^=readram(hl.word);
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor (hl)");
        break;
      case 0xaf: // xor a
        acc^=acc;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         xor a\t");
        break;
      case 0xb0: // or b
        acc|=bc.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or b\t");
        break;
      case 0xb1: // or c
        acc|=bc.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or c\t");
        break;
      case 0xb2: // or d
        acc|=de.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or d\t");
        break;
      case 0xb3: // or e
        acc|=de.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or e\t");
        break;
      case 0xb4: // or h
        acc|=hl.bytes.high;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or h\t");
        break;
      case 0xb5: // or l
        acc|=hl.bytes.low;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or l\t");
        break;
      case 0xb6: // or (hl)
        acc|=readram(hl.word);
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or (hl)");
        break;
      case 0xb7: // or a
        acc|=acc;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("         or a\t");
        break;
      case 0xb8: // cp b
        sub8(acc,bc.bytes.high);
        DEBUGPRINT("         cp b\t");
        break;
      case 0xb9: // cp c
        sub8(acc,bc.bytes.low);
        DEBUGPRINT("         cp c\t");
        break;
      case 0xba: // cp d
        sub8(acc,de.bytes.high);
        DEBUGPRINT("         cp d\t");
        break;
      case 0xbb: // cp e
        sub8(acc,de.bytes.low);
        DEBUGPRINT("         cp e\t");
        break;
      case 0xbc: // cp h
        sub8(acc,hl.bytes.high);
        DEBUGPRINT("         cp h\t");
        break;
      case 0xbd: // cp l
        sub8(acc,hl.bytes.low);
        DEBUGPRINT("         cp l\t");
        break;
      case 0xbe: // cp (hl)
        sub8(acc,readram(hl.word));
        DEBUGPRINT("         cp (hl)");
        break;
      case 0xbf: // cp a
        sub8(acc,acc);
        DEBUGPRINT("         cp a\t");
        break;
      case 0xc0: // ret nz
        if (!testflag(ZFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret nz\t");
        break;
      case 0xc1: // pop bc
        bc.word=popw();
        DEBUGPRINT("         pop bc\t");
        break;
      case 0xc2: // jp nz,xxxx
        tempw=fetchw();
        if (!testflag(ZFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp nz,");
        DEBUGPHEX16(tempw);
        break;
      case 0xc3: // jp xxxx
        pcreg=fetchw();
        DEBUGPRINT("   jp ");
        DEBUGPHEX16(pcreg);
        break;
      case 0xc4: // call nz,xxxx
        tempw=fetchw();
        if (!testflag(ZFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call nz,");
        DEBUGPHEX16(tempw);
        break;
      case 0xc5: // push bc
        pushw(bc.word);
        DEBUGPRINT("         push bc");
        break;
      case 0xc6: // add a,xx
        tempb=fetch();
        acc=add8(acc,tempb);
        DEBUGPRINT("      add a,");
        DEBUGPHEX(tempb);
        break;
      case 0xc7: // rst 00
        pushw(pcreg);
        pcreg=0;
        DEBUGPRINT("         rst 00\t");
        break;
      case 0xc8: // ret z
        if (testflag(ZFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret z\t");
        break;
      case 0xc9: // ret
        pcreg=popw();
        DEBUGPRINT("         ret\t");
        break;
      case 0xca: // jp z,xxxx
        tempw=fetchw();
        if (testflag(ZFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp z,");
        DEBUGPHEX16(tempw);
        break;
      case 0xcb: // BITS
        emulate_cb();
        break;
      case 0xcc: // call z,xxxx
        tempw=fetchw();
        if (testflag(ZFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call z,");
        DEBUGPHEX16(tempw);
        break;
      case 0xcd: // call xxxx
        tempw=fetchw();
        pushw(pcreg);
        pcreg=tempw;
        DEBUGPRINT("   call ");
        DEBUGPHEX16(tempw);
        break;
      case 0xce: // adc a,xx
        tempb=fetch();
        acc=adc8(acc,tempb);
        DEBUGPRINT("      adc a,");
        DEBUGPHEX(tempb);
        break;
      case 0xcf: // rst 08
        pushw(pcreg);
        pcreg=0x0008;
        DEBUGPRINT("         rst 08\t");
        break;
      case 0xd0: // ret nc
        if (!testflag(CFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret nc\t");
        break;
      case 0xd1: // pop de
        de.word=popw();
        DEBUGPRINT("         pop de\t");
        break;
      case 0xd2: // jp nc,xxxx
        tempw=fetchw();
        if (!testflag(CFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp nc,");
        DEBUGPHEX16(tempw);
        break;
      case 0xd3: // out (xx),a
        tempb=fetch();
        writeio(tempb,acc);
        DEBUGPRINT("      out (");
        DEBUGPHEX(tempb);
        DEBUGPRINT("),a");
        break;
      case 0xd4: // call nc,xxxx
        tempw=fetchw();
        if (!testflag(CFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call nc,");
        DEBUGPHEX16(tempw);
        break;
      case 0xd5: // push de
        pushw(de.word);
        DEBUGPRINT("         push de");
        break;
      case 0xd6: // sub xx
        tempb=fetch();
        acc=sub8(acc,tempb);
        DEBUGPRINT("      sub ");
        DEBUGPHEX(tempb);
        DEBUGPRINT("\t");
        break;
      case 0xd7: // rst 10
        pushw(pcreg);
        pcreg=0x0010;
        DEBUGPRINT("         rst 10\t");
        break;
      case 0xd8: // ret c
        if (testflag(CFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret c\t");
        break;
      case 0xd9: // exx
        swap(tempw,bc.word,bc2.word);
        swap(tempw,de.word,de2.word);
        swap(tempw,hl.word,hl2.word);
        DEBUGPRINT("         exx\t");
        break;
      case 0xda: // jp c,xxxx
        tempw=fetchw();
        if (testflag(CFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp c,");
        DEBUGPHEX16(tempw);
        break;
      case 0xdb: // in a,(xx)
        tempb=fetch();
        acc=readio(tempb);
        DEBUGPRINT("      in a,(");
        DEBUGPHEX(tempb);
        DEBUGPRINT(")");
        break;
      case 0xdc: // call c,xxxx
        tempw=fetchw();
        if (testflag(CFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call c,");
        DEBUGPHEX16(tempw);
        break;
      case 0xdd: // IX prefix
        if (!emulate_dd())
          goto ignoreprefix;
        break;
      case 0xde: // sbc a,xx
        tempb=fetch();
        acc=sbc8(acc,tempb);
        DEBUGPRINT("         sbc a,");
        DEBUGPHEX(tempb);
        break;
      case 0xdf: // rst 18
        pushw(pcreg);
        pcreg=0x0018;
        DEBUGPRINT("         rst 18\t");
        break;
      case 0xe0: // ret po
        if (!testflag(PVFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret po");
        break;
      case 0xe1: // pop hl
        hl.word=popw();
        DEBUGPRINT("         pop hl\t");
        break;
      case 0xe2: // jp po,xxxx
        tempw=fetchw();
        if (!testflag(PVFLAG))
          pcreg=tempw;
        DEBUGPRINT("         jp po,");
        DEBUGPHEX16(tempw);
        break;
      case 0xe3: // ex (sp),hl
        tempw=readram(spreg)|(((uint16_t)readram(spreg+1))<<8);
        writeram(spreg,hl.bytes.low);
        writeram(spreg+1,hl.bytes.high);
        hl.word=tempw;
        DEBUGPRINT("         ex (sp),hl");
        break;
      case 0xe4: // call po,xxxx
        tempw=fetchw();
        if (!testflag(PVFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call po,");
        DEBUGPHEX16(tempw);
        break;
      case 0xe5: // push hl
        pushw(hl.word);
        DEBUGPRINT("         push hl");
        break;
      case 0xe6: // and xx
        tempb=fetch();
        acc&=tempb;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG);
        setflags(HFLAG);
        DEBUGPRINT("      and ");
        DEBUGPHEX(tempb);
        DEBUGPRINT("\t");
        break;
      case 0xe7: // rst 20
        pushw(pcreg);
        pcreg=0x0020;
        DEBUGPRINT("         rst 20\t");
        break;
      case 0xe8: // ret pe
        if (testflag(PVFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret pe\t");
        break;
      case 0xe9: // jp (hl)
        pcreg=hl.word;
        DEBUGPRINT("         jp (hl)");
        break;
      case 0xea: // jp pe,xxxx
        tempw=fetchw();
        if (testflag(PVFLAG))
          pcreg=tempw;
        DEBUGPRINT("         jp pe,");
        DEBUGPHEX16(tempw);
        break;
      case 0xeb: // ex de,hl
        swap(tempw,de.word,hl.word);
        DEBUGPRINT("         ex de,hl");
        break;
      case 0xec: // call pe,xxxx
        tempw=fetchw();
        if (testflag(PVFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call pe,");
        DEBUGPHEX16(tempw);
        break;
      case 0xed: // EXTD prefix
        emulate_ed();
        break;
      case 0xee: // xor xx
        tempb=fetch();
        acc^=tempb;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("      xor ");
        DEBUGPHEX(tempb);
        DEBUGPRINT("\t");
        break;
      case 0xef: // rst 28
        pushw(pcreg);
        pcreg=0x0028;
        DEBUGPRINT("         rst 28\t");
        break;
      case 0xf0: // ret p
        if (!testflag(SFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret p\t");
        break;
      case 0xf1: // pop af
        flags=popb();
        acc=popb();
        DEBUGPRINT("         pop af\t");
        break;
      case 0xf2: // jp p,xxxx
        tempw=fetchw();
        if (!testflag(SFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp p,");
        DEBUGPHEX16(tempw);
        break;
      case 0xf3: // di
        iff1=iff2=false;
        DEBUGPRINT("         di\t");
        break;
      case 0xf4: // call p,xxxx
        tempw=fetchw();
        if (!testflag(SFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call p,");
        DEBUGPHEX16(tempw);
        break;
      case 0xf5: // push af
        pushb(acc);
        pushb(flags);
        DEBUGPRINT("         push af");
        break;
      case 0xf6: // or xx
        tempb=fetch();
        acc|=tempb;
        setlogicflags(acc);
        clearflags(CFLAG|NFLAG|HFLAG);
        DEBUGPRINT("      or ");
        DEBUGPHEX(tempb);
        DEBUGPRINT("\t");
        break;
      case 0xf7: // rst 30
        pushw(pcreg);
        pcreg=0x0030;
        DEBUGPRINT("         rst 30\t");
        break;
      case 0xf8: // ret m
        if (testflag(SFLAG))
          pcreg=popw();
        DEBUGPRINT("         ret m\t");
        break;
      case 0xf9: // ld sp,hl
        spreg=hl.word;
        DEBUGPRINT("         ld sp,hl");
        break;
      case 0xfa: // jp m,xxxx
        tempw=fetchw();
        if (testflag(SFLAG))
          pcreg=tempw;
        DEBUGPRINT("   jp m,");
        DEBUGPHEX16(tempw);
        break;
      case 0xfb: // ei
        iff1=iff2=true;
        DEBUGPRINT("         ei\t");
        break;
      case 0xfc: // call m,xxxx
        tempw=fetchw();
        if (testflag(SFLAG)) {
          pushw(pcreg);
          pcreg=tempw;
        }
        DEBUGPRINT("   call m,");
        DEBUGPHEX16(tempw);
        break;
      case 0xfd: // IY prefix
        if (!emulate_fd())
          goto ignoreprefix;
        break;
      case 0xfe: // cp xx
        tempb=fetch();
        sub8(acc,tempb);
        DEBUGPRINT("      cp ");
        DEBUGPHEX(tempb);
        DEBUGPRINT("\t");
        break;
      case 0xff: // rst 58
        pushw(pcreg);
        pcreg=0x0038;
        DEBUGPRINT("         rst 38\t");
        break;
      default:
        ERRORPRINT("invalid instruction ");
        ERRORPHEX(tempb);
        ERRORPRINT(" at ");
        ERRORPHEX16(pcreg-1);
        fault();
        break;
    }
    #ifdef INSTRUCTIONDEBUG
    DEBUGPRINT("\tA=");
    DEBUGPHEX(acc);
    DEBUGPRINT(" BC=");
    DEBUGPHEX16(bc.word);
    DEBUGPRINT(" DE=");
    DEBUGPHEX16(de.word);
    DEBUGPRINT(" HL=");
    DEBUGPHEX16(hl.word);
    DEBUGPRINT(" SP=");
    DEBUGPHEX16(spreg);
    DEBUGPRINT(" ");
    x=flags;
    for (tempb=0;tempb<8;tempb++) {
      DEBUGSEND(x&0x80?flagnames[tempb]:'_');
      x<<=1;
    }
    DEBUGPRINT("\r\n");
    if (DEBUGKEY())
      instdebugging=!instdebugging;
    #endif
    checkforinterrupts();
  }
}

// bit and rotate instructions
void z80::emulate_cb()
{
uint8_t b,v,x;
  b=fetch();
  switch (b&0x07) { // 3 lsb are register to operate on
    case 0:
      v=bc.bytes.high;
      break;
    case 1:
      v=bc.bytes.low;
      break;
    case 2:
      v=de.bytes.high;
      break;
    case 3:
      v=de.bytes.low;
      break;
    case 4:
      v=hl.bytes.high;
      break;
    case 5:
      v=hl.bytes.low;
      break;
    case 6:
      v=readram(hl.word);
      break;
    case 7:
      v=acc;
      break;
  }
  // 5 msb are instruction to perform
  switch (b>>3) {
    case 0: // rlc
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v<<=1;
      v|=carryflag();
      setlogicflags(v);
      DEBUGPRINT("      rlc ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;
    case 1: // rrc
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&1)
        setflags(CFLAG);
      v>>=1;
      if (testflag(CFLAG))
        v|=0x80;
      setlogicflags(v);
      DEBUGPRINT("      rrc ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;      
    case 2: // rl
      clearflags(NFLAG|HFLAG);
      x=v;
      v=(v<<1)|carryflag();
      if (x&0x80)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      DEBUGPRINT("      rl ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;      
    case 3: // rr
      clearflags(NFLAG|HFLAG);
      x=v;
      v=(v>>1);
      if (testflag(CFLAG))
        v|=0x80;
      if (x&0x01)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      DEBUGPRINT("      rr ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;      
    case 4: // sla
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=v<<1;
      setlogicflags(v);
      DEBUGPRINT("      sla ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;      
    case 5: // sra
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=(v&0x80)|(v>>1);
      setlogicflags(v);
      DEBUGPRINT("      sra ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;
    case 6: // sll
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=(v<<1)|1;
      setlogicflags(v);
      DEBUGPRINT("      sll ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;      
    case 7: // srl
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=v>>1;
      setlogicflags(v);
      DEBUGPRINT("      srl ");
      DEBUGPRINT(rnames[b&7]);
      DEBUGPRINT("\t");
      break;
    case 8: // bit 0
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x01))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 0,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 9: // bit 1
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x02))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 1,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 10: // bit 2
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x04))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 2,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 11: // bit 3
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x08))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 3,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 12: // bit 4
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x10))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 4,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 13: // bit 5
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x20))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 5,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 14: // bit 6
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x40))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 6,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 15: // bit 7
      clearflags(ZFLAG|NFLAG);
      if (!(v&0x80))
        setflags(ZFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      bit 7,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 16: // res 0
      v&=~(0x01);
      DEBUGPRINT("      res 0,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 17: // res 1
      v&=~(0x02);
      DEBUGPRINT("      res 1,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 18: // res 2
      v&=~(0x04);
      DEBUGPRINT("      res 2,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 19: // res 3
      v&=~(0x08);
      DEBUGPRINT("      res 3,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 20: // res 4
      v&=~(0x10);
      DEBUGPRINT("      res 4,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 21: // res 5
      v&=~(0x20);
      DEBUGPRINT("      res 5,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 22: // res 6
      v&=~(0x40);
      DEBUGPRINT("      res 6,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 23: // res 7
      v&=~(0x80);
      DEBUGPRINT("      res 7,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 24: // set 0
      v|=0x01;
      DEBUGPRINT("      set 0,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 25: // set 1
      v|=0x02;
      DEBUGPRINT("      set 1,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 26: // set 2
      v|=0x04;
      DEBUGPRINT("      set 2,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 27: // set 3
      v|=0x08;
      DEBUGPRINT("      set 3,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 28: // set 4
      v|=0x10;
      DEBUGPRINT("      set 4,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 29: // set 5
      v|=0x20;
      DEBUGPRINT("      set 5,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 30: // set 6
      v|=0x40;
      DEBUGPRINT("      set 6,");
      DEBUGPRINT(rnames[b&7]);
      break;
    case 31: // set 7
      v|=0x80;
      DEBUGPRINT("      set 7,");
      DEBUGPRINT(rnames[b&7]);
      break;
  }
  switch (b&0x07) { // 3 lsb are register to operate on
    case 0:
      bc.bytes.high=v;
      break;
    case 1:
      bc.bytes.low=v;
      break;
    case 2:
      de.bytes.high=v;
      break;
    case 3:
      de.bytes.low=v;
      break;
    case 4:
      hl.bytes.high=v;
      break;
    case 5:
      hl.bytes.low=v;
      break;
    case 6:
      writeram(hl.word,v);
      break;
    case 7:
      acc=v;
      break;
  }
}

void z80::emulate_ed()
{
uint8_t o;
  tempb=fetch();
  switch (tempb) {
    case 0x40: // in b,(c)
      bc.bytes.high=readio(bc.bytes.low);
      setlogicflags(bc.bytes.high);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in b,(c)");
      break;
    case 0x41: // out (c),b
      writeio(bc.bytes.low,bc.bytes.high);
      DEBUGPRINT("      out (c),b");
      break;
    case 0x42: // sbc hl,bc
      hl.word=sbc16(hl.word,bc.word);
      DEBUGPRINT("      sbc hl,bc");
      break;
    case 0x43: // ld (xxxx),bc
      tempw=fetchw();
      writeram(tempw,bc.bytes.low);
      writeram(tempw+1,bc.bytes.high);
      DEBUGPRINT("ld (");
      DEBUGPHEX16(tempw);
      DEBUGPRINT("),bc");
      break;
    case 0x44: // neg
      tempb=sub8(0,acc);
      if (acc)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      acc=tempb;
      DEBUGPRINT("      neg\t");
      break;
    case 0x45: // retn
      iff1=iff2;
      pcreg=popw();
      DEBUGPRINT("      retn\t");
      break;
    case 0x46: // im 0
      im=0;
      DEBUGPRINT("      im 0\t");
      break;
    case 0x47: // ld i,a
      ir.bytes.high=acc;
      DEBUGPRINT("      ld i,a\t");
      break;
    case 0x48: // in c,(c)
      bc.bytes.low=readio(bc.bytes.low);
      setlogicflags(bc.bytes.low);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in c,(c)");
      break;
    case 0x49: // out (c),c
      writeio(bc.bytes.low,bc.bytes.low);
      DEBUGPRINT("      out (c),c");
      break;
    case 0x4a: // adc hl,bc
      hl.word=adc16(hl.word,bc.word);
      DEBUGPRINT("      adc hl,bc");
      break;
    case 0x4b: // ld bc,(xxxx)
      tempw=fetchw();
      bc.bytes.low=readram(tempw);
      bc.bytes.high=readram(tempw+1);
      DEBUGPRINT("ld bc,(");
      DEBUGPHEX16(tempw);
      DEBUGPRINT(")");
      break;
    case 0x4d: // reti
      pcreg=popw();
      //RETI is like regular RET for Z80 but peripheral chips also
      //decode it for resetting interrupt daisy chain. as we dont have full
      //bus with M1 signalling and no interrupt support, it does not matter
      DEBUGPRINT("      reti\t");
      break;
    case 0x4f: // ld r,a
      ir.bytes.low=acc;
      // msb of r register should be preserved, as it does not count
      // but for efficiency 8-bit counter is used in this emulator.
      // as its not trying to be exact Z80 replica for any particular
      // machine, saving a few bytes of code by ignoring the issue here
      DEBUGPRINT("      ld r,a\t");
      break;
    case 0x50: // in d,(c)
      de.bytes.high=readio(bc.bytes.low);
      setlogicflags(de.bytes.high);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in d,(c)");
      break;
    case 0x51: // out (c),d
      writeio(bc.bytes.low,de.bytes.high);
      DEBUGPRINT("      out (c),d");
      break;
    case 0x52: // sbc hl,de
      hl.word=sbc16(hl.word,de.word);
      DEBUGPRINT("      sbc hl,de");
      break;
    case 0x53: // ld (xxxx),de
      tempw=fetchw();
      writeram(tempw,de.bytes.low);
      writeram(tempw+1,de.bytes.high);
      DEBUGPRINT("ld (");
      DEBUGPHEX16(tempw);
      DEBUGPRINT("),de");
      break;
    case 0x56: // im 1
      im=1;
      DEBUGPRINT("      im 1\t");
      break;
    case 0x57: // ld a,i
      acc=ir.bytes.high;
      setlogicflags(acc);
      if (iff2)
        setflags(PVFLAG);
      else
        clearflags(PVFLAG);
      DEBUGPRINT("      ld a,i\t");
      break;
    case 0x58: // in e,(c)
      de.bytes.low=readio(bc.bytes.low);
      setlogicflags(de.bytes.low);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in e,(c)");
      break;
    case 0x59: // out (c),e
      writeio(bc.bytes.low,de.bytes.low);
      DEBUGPRINT("      out (c),e");
      break;
    case 0x5a: // adc hl,de
      hl.word=adc16(hl.word,de.word);
      DEBUGPRINT("      adc hl,de");
      break;
    case 0x5b: // ld de,(xxxx)
      tempw=fetchw();
      de.bytes.low=readram(tempw);
      de.bytes.high=readram(tempw+1);
      DEBUGPRINT("ld de,(");
      DEBUGPHEX16(tempw);
      DEBUGPRINT(")");
      break;
    case 0x5e: // im 2
      im=2;
      DEBUGPRINT("      im 2\t");
      break;
    case 0x5f: // ld a,r
      acc=ir.bytes.low;
      setlogicflags(acc);
      if (iff2)
        setflags(PVFLAG);
      else
        clearflags(PVFLAG);
      DEBUGPRINT("      ld a,r\t");
      break;
    case 0x60: // in h,(c)
      hl.bytes.high=readio(bc.bytes.low);
      setlogicflags(hl.bytes.high);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in h,(c)");
      break;
    case 0x61: // out (c),h
      writeio(bc.bytes.low,hl.bytes.high);
      DEBUGPRINT("      out (c),h");
      break;
    case 0x62: // sbc hl,hl
      hl.word=sbc16(hl.word,hl.word);
      DEBUGPRINT("      sbc hl,hl");
      break;
    case 0x67: // rrd
      tempw=readram(hl.word)|((uint16_t)acc<<8);
      acc=(acc&0xf0)|(tempw&0x0f);
      tempb=tempw>>4;
      writeram(hl.word,tempb);
      setlogicflags(acc);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      rrd\t");
      break;
    case 0x68: // in l,(c)
      hl.bytes.low=readio(bc.bytes.low);
      setlogicflags(hl.bytes.low);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in l,(c)");
      break;
    case 0x69: // out (c),l
      writeio(bc.bytes.low,hl.bytes.low);
      DEBUGPRINT("      out (c),l");
      break;
    case 0x6a: // adc hl,hl
      hl.word=adc16(hl.word,hl.word);
      DEBUGPRINT("      adc hl,hl");
      break;
    case 0x6f: // rld
      tempw=readram(hl.word)|((uint16_t)acc<<8);
      acc=(acc&0xf0)|((tempw&0xf0)>>4);
      tempb=(tempw<<4)|((tempw>>8)&0x0f);
      writeram(hl.word,tempb);
      setlogicflags(acc);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      rld\t");
      break;
    case 0x72: // sbc hl,sp
      hl.word=sbc16(hl.word,spreg);
      DEBUGPRINT("      sbc hl,sp");
      break;
    case 0x73: // ld (xxxx),sp
      tempw=fetchw();
      writeram(tempw,spreg&255);
      writeram(tempw+1,spreg>>8);
      DEBUGPRINT("ld (");
      DEBUGPHEX16(tempw);
      DEBUGPRINT("),sp");
      break;
    case 0x78: // in a,(c)
      acc=readio(bc.bytes.low);
      setlogicflags(acc);
      clearflags(HFLAG|NFLAG);
      DEBUGPRINT("      in a,(c)");
      break;
    case 0x79: // out (c),a
      writeio(bc.bytes.low,acc);
      DEBUGPRINT("      out (c),a");
      break;
    case 0x7a: // adc hl,sp
      hl.word=adc16(hl.word,spreg);
      DEBUGPRINT("      adc hl,sp");
      break;
    case 0x7b: // ld sp,(xxxx)
      tempw=fetchw();
      spreg=readram(tempw);
      spreg|=(uint16_t)(readram(tempw+1))<<8;
      DEBUGPRINT("ld sp,(");
      DEBUGPHEX16(tempw);
      DEBUGPRINT(")");
      break;
    case 0xa0: // ldi
      tempb=readram(hl.word);
      writeram(de.word,tempb);
      hl.word++;
      de.word++;
      bc.word--;
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      clearflags(NFLAG|HFLAG);
      DEBUGPRINT("      ldi\t");
      break;
    case 0xa1: // cpi
      tempb=readram(hl.word);
      o=flags&CFLAG;
      sub8(acc,tempb);
      hl.word++;
      bc.word--;
      clearflags(CFLAG);
      setflags(o|NFLAG);
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      DEBUGPRINT("      cpi\t");
      break;
    case 0xa2: // ini
      tempb=readio(bc.bytes.low);
      writeram(hl.word,tempb);
      hl.word++;
      bc.bytes.high--;
      setflags(NFLAG);
      if (bc.bytes.high)
        clearflags(ZFLAG);
      else
        setflags(ZFLAG);
      DEBUGPRINT("      ini\t");
      break;
    case 0xa3: // outi
      tempb=readram(hl.word);
      writeio(bc.bytes.low,tempb);
      hl.word++;
      bc.bytes.high--;
      setflags(NFLAG);
      if (bc.bytes.high)
        clearflags(ZFLAG);
      else
        setflags(ZFLAG);
      DEBUGPRINT("      outi\t");
      break;
    case 0xa8: // ldd
      tempb=readram(hl.word);
      writeram(de.word,tempb);
      hl.word--;
      de.word--;
      bc.word--;
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      clearflags(NFLAG|HFLAG);
      DEBUGPRINT("      ldd\t");
      break;
    case 0xa9: // cpd
      o=flags&CFLAG;
      tempb=readram(hl.word);
      sub8(acc,tempb);
      hl.word--;
      bc.word--;
      clearflags(CFLAG);
      setflags(NFLAG|o);
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      DEBUGPRINT("      cpd\t");
      break;
    case 0xaa: // ind
      tempb=readio(bc.bytes.low);
      writeram(hl.word,tempb);
      hl.word--;
      bc.bytes.high--;
      setflags(NFLAG);
      if (bc.bytes.high)
        clearflags(ZFLAG);
      else
        setflags(ZFLAG);
      DEBUGPRINT("      ind\t");
      break;
    case 0xab: // outd
      tempb=readram(hl.word);
      writeio(bc.bytes.low,tempb);
      hl.word--;
      bc.bytes.high--;
      setflags(NFLAG);
      if (bc.bytes.high)
        clearflags(ZFLAG);
      else
        setflags(ZFLAG);
      DEBUGPRINT("      outd\t");
      break;
    case 0xb0: // ldir
      do {
        tempb=readram(hl.word);
        writeram(de.word,tempb);
        hl.word++;
        de.word++;
        bc.word--;
        checkforinterrupts();
      } while (bc.word!=0);
      clearflags(PVFLAG|HFLAG|NFLAG);
      DEBUGPRINT("      ldir\t");
      break;
    case 0xb1: // cpir
      o=flags&CFLAG;
      do {
        tempb=readram(hl.word);
        sub8(acc,tempb);
        hl.word++;
        bc.word--;
        checkforinterrupts();
      } while (bc.word && !testflag(ZFLAG));
      clearflags(CFLAG);
      setflags(NFLAG|o);
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      DEBUGPRINT("      cpir\t");
      break;
    case 0xb2: // inir
      do {
        tempb=readio(bc.bytes.low);
        writeram(hl.word,tempb);
        hl.word++;
        bc.bytes.high--;
        checkforinterrupts();
      } while (bc.bytes.high);
      setflags(ZFLAG);
      clearflags(NFLAG);      
      DEBUGPRINT("      inir\t");
      break;
    case 0xb3: // otir
      do {
        tempb=readram(hl.word);
        writeio(bc.bytes.low,tempb);
        hl.word++;
        bc.bytes.high--;
        checkforinterrupts();
      } while (bc.bytes.high);
      setflags(ZFLAG|NFLAG);
      DEBUGPRINT("      otir\t");
      break;
    case 0xb8: // lddr
      do {
        tempb=readram(hl.word);
        writeram(de.word,tempb);
        hl.word--;
        de.word--;
        bc.word--;
        checkforinterrupts();
      } while (bc.word!=0);
      clearflags(PVFLAG|HFLAG|NFLAG);
      DEBUGPRINT("      lddr\t");
      break;
    case 0xb9: // cpdr
      o=flags&CFLAG;
      do {
        tempb=readram(hl.word);
        sub8(acc,tempb);
        hl.word--;
        bc.word--;
        checkforinterrupts();
      } while (bc.word!=0 && !testflag(ZFLAG));
      clearflags(CFLAG);
      setflags(NFLAG|o);
      if (!bc.word)
        clearflags(PVFLAG);
      else
        setflags(PVFLAG);
      DEBUGPRINT("      cpdr\t");
      break;
    case 0xba: // indr
      do {
        tempb=readio(bc.bytes.low);
        writeram(hl.word,tempb);
        hl.word--;
        bc.bytes.high--;
        checkforinterrupts();
      } while (bc.bytes.high!=0);
      setflags(ZFLAG|NFLAG);
      DEBUGPRINT("      indr\t");
      break;
    case 0xbb: // otdr 
      do {
        tempb=readram(hl.word);
        writeio(bc.bytes.low,tempb);
        hl.word--;
        bc.bytes.high--;
        checkforinterrupts();
      } while (bc.bytes.high!=0);
      setflags(ZFLAG|NFLAG);
      DEBUGPRINT("      otdr\t");
      break;
    default:
      ERRORPRINT("invalid instruction ED ");
      ERRORPHEX(tempb);
      ERRORPRINT(" at ");
      ERRORPHEX16(pcreg-2);
      fault();
      break;
  }
}

// IX instructions
bool z80::emulate_dd()
{
uint8_t o;
  tempb=fetch();
  switch (tempb) {
    case 0x09: // add ix,bc
      ix.word=add16(ix.word,bc.word);
      DEBUGPRINT("      add ix,bc");
      break;
    case 0x19: // add ix,de
      ix.word=add16(ix.word,de.word);
      DEBUGPRINT("      add ix,de");
      break;
    case 0x21: // ld ix,xxxx
      ix.word=fetchw();
      DEBUGPRINT("ld ix,");
      DEBUGPHEX16(ix.word);
      break;
    case 0x22: // ld (xxxx),ix
      tempw=fetchw();
      writeram(tempw,ix.bytes.low);
      writeram(tempw+1,ix.bytes.high);
      DEBUGPRINT("ld (");
      DEBUGPHEX16(tempw);
      DEBUGPRINT("),ix");
      break;
    case 0x23: // inc ix
      ix.word++;
      DEBUGPRINT("      inc ix\t");
      break;
    case 0x24: // inc ixh
      ix.bytes.high=inc8(ix.bytes.high);
      DEBUGPRINT("      inc ixh");
      break;
    case 0x25: // dec ixh
      ix.bytes.high=dec8(ix.bytes.high);
      DEBUGPRINT("      dec ixh");
      break;
      break;
    case 0x26: // ld ixh,xx
      ix.bytes.high=fetch();
      DEBUGPRINT("  ld ixh,");
      DEBUGPHEX(ix.bytes.high);
      break;
    case 0x29: // add ix,ix
      ix.word=add16(ix.word,ix.word);
      DEBUGPRINT("      add ix,ix");
      break;
    case 0x2a: // ld ix,(xxxx)
      tempw=fetchw();
      ix.bytes.low=readram(tempw);
      ix.bytes.high=readram(tempw+1);
      DEBUGPRINT("ld ix,(");
      DEBUGPHEX16(tempw);
      DEBUGPRINT(")");
      break;
    case 0x2b: // dec ix
      ix.word--;
      DEBUGPRINT("      dec ix\t");
      break;
    case 0x2c: // inc ixl
      ix.bytes.low=inc8(ix.bytes.low);
      DEBUGPRINT("      inc ixl");
      break;
      break;
    case 0x2d: // dec ixl
      ix.bytes.low=dec8(ix.bytes.low);
      DEBUGPRINT("      dec ixl");
      break;
    case 0x2e: // ld ixl,xx
      ix.bytes.low=fetch();
      DEBUGPRINT("  ld ixl,");
      DEBUGPHEX(ix.bytes.low);
      break;
    case 0x34: // inc (ix+xx)
      o=fetch();
      tempw=ix.word+o;
      tempb=readram(tempw);
      tempb=inc8(tempb);
      writeram(tempw,tempb);
      DEBUGPRINT("   inc (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x35: // dec (ix+xx)
      o=fetch();
      tempw=ix.word+o;
      tempb=readram(tempw);
      tempb=dec8(tempb);
      writeram(tempw,tempb);
      DEBUGPRINT("   dec (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x36: // ld (ix+xx),xx
      o=fetch();
      tempb=fetch();
      writeram(ix.word+o,tempb);
      DEBUGPRINT("ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),");
      DEBUGPHEX(tempb);
      break;
    case 0x39: // add ix,sp
      ix.word=add16(ix.word,spreg);
      DEBUGPRINT("      add ix,sp");
      break;
    case 0x44: // ld b,ixh
      bc.bytes.high=ix.bytes.high;
      DEBUGPRINT("      ld b,ixh");
      break;
    case 0x45: // ld b,ixl
      bc.bytes.high=ix.bytes.low;
      DEBUGPRINT("      ld b,ixl");
      break;
    case 0x46: // ld b,(ix+xx)
      o=fetch();
      bc.bytes.high=readram(ix.word+o);
      DEBUGPRINT("   ld b,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x4c: // ld c,ixh
      bc.bytes.low=ix.bytes.high;
      DEBUGPRINT("      ld c,ixh");
      break;
    case 0x4d: // ld c,ixl
      bc.bytes.low=ix.bytes.low;
      DEBUGPRINT("      ld c,ixl");
      break;
    case 0x4e: // ld c,(ix+xx)
      o=fetch();
      bc.bytes.low=readram(ix.word+o);
      DEBUGPRINT("   ld c,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x54: // ld d,ixh
      de.bytes.high=ix.bytes.high;
      DEBUGPRINT("      ld d,ixh");
      break;
    case 0x55: // ld d,ixl
      de.bytes.high=ix.bytes.low;
      DEBUGPRINT("      ld d,ixl");
      break;
    case 0x56: // ld d,(ix+xx)
      o=fetch();
      de.bytes.high=readram(ix.word+o);
      DEBUGPRINT("   ld d,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x5c: // ld e,ixh
      de.bytes.low=ix.bytes.high;
      DEBUGPRINT("      ld e,ixh");
      break;
    case 0x5d: // ld e,ixl
      de.bytes.low=ix.bytes.low;
      DEBUGPRINT("      ld e,ixl");
      break;
    case 0x5e: // ld e,(ix+xx)
      o=fetch();
      de.bytes.low=readram(ix.word+o);
      DEBUGPRINT("   ld e,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x60: // ld ixh,b
      ix.bytes.high=bc.bytes.high;
      DEBUGPRINT("      ld ixh,b");
      break;
    case 0x61: // ld ixh,c
      ix.bytes.high=bc.bytes.low;
      DEBUGPRINT("      ld ixh,c");
      break;
    case 0x62: // ld ixh,d
      ix.bytes.high=de.bytes.high;
      DEBUGPRINT("      ld ixh,d");
      break;
    case 0x63: // ld ixh,e
      ix.bytes.high=de.bytes.low;
      DEBUGPRINT("      ld ixh,e");
      break;
    case 0x64: // ld ixh,ixh
      DEBUGPRINT("      ld ixh,ixh");
      break;
    case 0x65: // ld ixh,ixl
      ix.bytes.high=ix.bytes.low;
      DEBUGPRINT("      ld ixh,ixl");
      break;
    case 0x66: // ld h,(ix+xx)
      o=fetch();
      hl.bytes.high=readram(ix.word+o);
      DEBUGPRINT("   ld h,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x67: // ld ixh,a
      ix.bytes.high=acc;
      DEBUGPRINT("      ld ixh,a");
      break;
    case 0x68: // ld ixl,b
      ix.bytes.low=bc.bytes.high;
      DEBUGPRINT("      ld ixl,b");
      break;
    case 0x69: // ld ixl,c
      ix.bytes.low=bc.bytes.low;
      DEBUGPRINT("      ld ixl,c");
      break;
    case 0x6a: // ld ixl,d
      ix.bytes.low=de.bytes.high;
      DEBUGPRINT("      ld ixl,d");
      break;
    case 0x6b: // ld ixl,e
      ix.bytes.low=de.bytes.low;
      DEBUGPRINT("      ld ixl,e");
      break;
    case 0x6c: // ld ixl,ixh
      ix.bytes.low=ix.bytes.high;
      DEBUGPRINT("      ld ixl,ixh");
      break;
    case 0x6d: // ld ixl,ixl
      DEBUGPRINT("      ld ixl,ixl");
      break;
    case 0x6e: // ld l,(ix+xx)
      o=fetch();
      hl.bytes.low=readram(ix.word+o);
      DEBUGPRINT("   ld l,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x6f: // ld ixl,a
      ix.bytes.low=acc;
      DEBUGPRINT("      ld ixl,a");
      break;
    case 0x70: // ld (ix+xx),b
      o=fetch();
      writeram(ix.word+o,bc.bytes.high);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),b");
      break;
    case 0x71: // ld (ix+xx),c
      o=fetch();
      writeram(ix.word+o,bc.bytes.low);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),c");
      break;
    case 0x72: // ld (ix+xx),d
      o=fetch();
      writeram(ix.word+o,de.bytes.high);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),d");
      break;
    case 0x73: // ld (ix+xx),e
      o=fetch();
      writeram(ix.word+o,de.bytes.low);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),e");
      break;
    case 0x74: // ld (ix+xx),h
      o=fetch();
      writeram(ix.word+o,hl.bytes.high);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),h");
      break;
    case 0x75: // ld (ix+xx),l
      o=fetch();
      writeram(ix.word+o,hl.bytes.low);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),l");
      break;
    case 0x77: // ld (ix+xx),a
      o=fetch();
      writeram(ix.word+o,acc);
      DEBUGPRINT("   ld (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT("),a");
      break;
    case 0x7c: // ld a,ixh
      acc=ix.bytes.high;
      DEBUGPRINT("      ld a,ixh");
      break;
    case 0x7d: // ld a,ixl
      acc=ix.bytes.low;
      DEBUGPRINT("      ld a,ixl");
      break;
    case 0x7e: // ld a,(ix+xx)
      o=fetch();
      acc=readram(ix.word+o);
      DEBUGPRINT("   ld a,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x7f:
      break;
    case 0x84: // add a,ixh
      acc=add8(acc,ix.bytes.high);
      DEBUGPRINT("      add a,ixh");
      break;
    case 0x85: // add a,ixl
      acc=add8(acc,ix.bytes.low);
      DEBUGPRINT("      add a,ixl");
      break;
    case 0x86: // add a,(ix+xx)
      o=fetch();
      acc=add8(acc,readram(ix.word+o));
      DEBUGPRINT("   add a,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x8c: // adc a,ixh
      acc=adc8(acc,ix.bytes.high);
      DEBUGPRINT("      adc a,ixh");
      break;
    case 0x8d: // adc a,ixl
      acc=adc8(acc,ix.bytes.low);
      DEBUGPRINT("      adc a,ixl");
      break;
    case 0x8e: // adc a,(ix+xx)
      o=fetch();
      acc=adc8(acc,readram(ix.word+o));
      DEBUGPRINT("   adc a,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x94: // sub ixh
      acc=sub8(acc,ix.bytes.high);
      DEBUGPRINT("      sub ixh");
      break;
    case 0x95: // sub ixl
      acc=sub8(acc,ix.bytes.low);
      DEBUGPRINT("      sub ixl");
      break;
    case 0x96: // sub (ix+xx)
      o=fetch();
      acc=sub8(acc,readram(ix.word+o));
      DEBUGPRINT("   sub (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x9c: // sbc a,ixh
      acc=sbc8(acc,ix.bytes.high);
      DEBUGPRINT("      sbc a,ixh");
      break;
    case 0x9d: // sbc a,ixl
      acc=sbc8(acc,ix.bytes.low);
      DEBUGPRINT("      sbc a,ixl");
      break;
    case 0x9e: // sbc a,(ix+xx)
      o=fetch();
      acc=sbc8(acc,readram(ix.word+o));
      DEBUGPRINT("   sbc a,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xa4: // and ixh
      acc&=ix.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      and ixh");
      break;
    case 0xa5: // and l
      acc&=ix.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      and ixl");
      break;
    case 0xa6: // and (ix+xx)
      o=fetch();
      acc&=readram(ix.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("   and (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xac: // xor ixh
      acc^=ix.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      xor ixh");
      break;
    case 0xad: // xor ixl
      acc^=ix.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      xor ixl");
      break;
    case 0xae: // xor (ix+xx)
      o=fetch();
      acc^=readram(ix.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("   xor (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xb4: // or ixh
      acc|=ix.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      or ixh");
      break;
    case 0xb5: // or ixl
      acc|=ix.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      or ixl");
      break;
    case 0xb6: // or (ix+xx)
      o=fetch();
      acc|=readram(ix.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("   or (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xbc: // cp ixh
      sub8(acc,ix.bytes.high);
      DEBUGPRINT("      cp ixh");
      break;
    case 0xbd: // cp ixl
      sub8(acc,ix.bytes.low);
      DEBUGPRINT("      cp ixl");
      break;
    case 0xbe: // cp (ix+xx)
      o=fetch();
      sub8(acc,readram(ix.word+o));
      DEBUGPRINT("   cp (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xcb:
      emulate_ddcb(); // ix bits
      break;
    case 0xe1: // pop ix
      ix.word=popw();
      DEBUGPRINT("      pop ix");
      break;
    case 0xe3: // ex (sp),ix
      tempb=readram(spreg);
      writeram(spreg,ix.bytes.low);
      ix.bytes.low=tempb;
      tempb=readram(spreg+1);
      writeram(spreg+1,ix.bytes.high);
      ix.bytes.high=tempb;
      DEBUGPRINT("      ex (sp),ix");
      break;
    case 0xe5: // push ix
      pushw(ix.word);
      DEBUGPRINT("      push ix");
      break;
    case 0xe9: // jp (ix)
      pcreg=ix.word;
      DEBUGPRINT("      jp (ix)");
      break;
    case 0xf9: // ld sp,ix
      spreg=ix.word;
      DEBUGPRINT("      ld sp,ix");
      break;
    default:
      //ERRORPRINT("invalid instruction DD ");
      //ERRORPHEX(tempb);
      //ERRORPRINT(" at ");
      //ERRORPHEX16(pcreg-2);
      return false;
  }
  return true;
}

void z80::emulate_ddcb()
{
uint8_t b,v,o;
uint16_t w;
  o=fetch();
  b=fetch();
  w=o+ix.word; // all instructions take the index, even undocumented
  v=readram(w);
  switch (b) {
    case 0x06: // rlc (ix+xx)
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v<<=1;
      v|=carryflag();
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rlc (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x0e: // rrc (ix+xx)
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&1)
        setflags(CFLAG);
      v>>=1;
      if (testflag(CFLAG))
        v|=0x80;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rrc (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x16: // rl (ix+xx)
      clearflags(NFLAG|HFLAG);
      b=v;
      v=(v<<1)|carryflag();
      if (b&0x80)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rl (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x1e: // rr (ix+xx)
      clearflags(NFLAG|HFLAG);
      b=v;
      v=(v>>1);
      if (testflag(CFLAG))
        v|=0x80;
      if (b&0x01)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rr (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x26: // sla (ix+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=v<<1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sla (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x2e: // sra (ix+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=(v&0x80)|(v>>1);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sra (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x36: // sll (ix+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=(v<<1)|1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sll (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x3e: // srl (ix+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=v>>1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("srl (ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x46: // bit 0,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x01))
        setflags(ZFLAG);
      DEBUGPRINT("bit 0,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x4e: // bit 1,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x02))
        setflags(ZFLAG);
      DEBUGPRINT("bit 1,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x56: // bit 2,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x04))
        setflags(ZFLAG);
      DEBUGPRINT("bit 2,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x5e: // bit 3,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x08))
        setflags(ZFLAG);
      DEBUGPRINT("bit 3,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x66: // bit 4,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x10))
        setflags(ZFLAG);
      DEBUGPRINT("bit 4,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x6e: // bit 5,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x20))
        setflags(ZFLAG);
      DEBUGPRINT("bit 5,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x76: // bit 6,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x40))
        setflags(ZFLAG);
      DEBUGPRINT("bit 6,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x7e: // bit 7,(ix+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x80))
        setflags(ZFLAG);
      DEBUGPRINT("bit 7,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x86: // res 0,(ix+xx)
      v&=~(0x01);
      writeram(w,v);
      DEBUGPRINT("res 0,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x8e: // res 1,(ix+xx)
      v&=~(0x02);
      writeram(w,v);
      DEBUGPRINT("res 1,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x96: // res 2,(ix+xx)
      v&=~(0x04);
      writeram(w,v);
      DEBUGPRINT("res 2,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x9e: // res 3,(ix+xx)
      v&=~(0x08);
      writeram(w,v);
      DEBUGPRINT("res 3,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xa6: // res 4,(ix+xx)
      v&=~(0x10);
      writeram(w,v);
      DEBUGPRINT("res 4,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xae: // res 5,(ix+xx)
      v&=~(0x20);
      writeram(w,v);
      DEBUGPRINT("res 5,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xb6: // res 6,(ix+xx)
      v&=~(0x40);
      writeram(w,v);
      DEBUGPRINT("res 6,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xbe: // res 7,(ix+xx)
      v&=~(0x80);
      writeram(w,v);
      DEBUGPRINT("res 7,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xc6: // set 0,(ix+xx)
      v|=0x01;
      writeram(w,v);
      DEBUGPRINT("set 0,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xce: // set 1,(ix+xx)
      v|=0x02;
      writeram(w,v);
      DEBUGPRINT("set 1,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xd6: // set 2,(ix+xx)
      v|=0x04;
      writeram(w,v);
      DEBUGPRINT("set 2,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xde: // set 3,(ix+xx)
      v|=0x08;
      writeram(w,v);
      DEBUGPRINT("set 3,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xe6: // set 4,(ix+xx)
      v|=0x10;
      writeram(w,v);
      DEBUGPRINT("set 4,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xee: // set 5,(ix+xx)
      v|=0x20;
      writeram(w,v);
      DEBUGPRINT("set 5,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xf6: // set 6,(ix+xx)
      v|=0x40;
      writeram(w,v);
      DEBUGPRINT("set 6,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xfe: // set 7,(ix+xx)
      v|=0x80;
      writeram(w,v);
      DEBUGPRINT("set 7,(ix+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    default:
      ERRORPRINT("invalid instruction DD CB ");
      ERRORPHEX(o);
      ERRORPRINT(" ");
      ERRORPHEX(b);
      ERRORPRINT(" at ");
      ERRORPHEX16(pcreg-3);
      fault();
      break;
  }
}

// IY instructions
bool z80::emulate_fd()
{
uint8_t o;
  tempb=fetch();
  switch (tempb) {
    case 0x09: // add iy,bc
      iy.word=add16(iy.word,bc.word);
      DEBUGPRINT("      add iy,bc");
      break;
    case 0x19: // add iy,de
      iy.word=add16(iy.word,de.word);
      DEBUGPRINT("      add iy,de");
      break;
    case 0x21: // ld iy,xxxx
      iy.word=fetchw();
      DEBUGPRINT("ld iy,");
      DEBUGPHEX16(iy.word);
      break;
    case 0x22: // ld (xxxx),iy
      tempw=fetchw();
      writeram(tempw,iy.bytes.low);
      writeram(tempw+1,iy.bytes.high);
      DEBUGPRINT("ld (");
      DEBUGPHEX16(tempw);
      DEBUGPRINT("),iy");
      break;
    case 0x23: // inc iy
      iy.word++;
      DEBUGPRINT("      inc iy\t");
      break;
    case 0x24: // inc iyh
      iy.bytes.high=inc8(iy.bytes.high);
      DEBUGPRINT("      inc iyh");
      break;
    case 0x25: // dec iyh
      iy.bytes.high=dec8(iy.bytes.high);
      DEBUGPRINT("      dec iyh");
      break;
      break;
    case 0x26: // ld iyh,xx
      iy.bytes.high=fetch();
      DEBUGPRINT("  ld iyh,");
      DEBUGPHEX(iy.bytes.high);
      break;
    case 0x29: // add iy,iy
      iy.word=add16(iy.word,iy.word);
      DEBUGPRINT("      add iy,iy");
      break;
    case 0x2a: // ld iy,(xxxx)
      tempw=fetchw();
      iy.bytes.low=readram(tempw);
      iy.bytes.high=readram(tempw+1);
      DEBUGPRINT("ld iy,(");
      DEBUGPHEX16(tempw);
      DEBUGPRINT(")");
      break;
    case 0x2b: // dec iy
      iy.word--;
      DEBUGPRINT("      dec iy\t");
      break;
    case 0x2c: // inc iyl
      iy.bytes.low=inc8(iy.bytes.low);
      DEBUGPRINT("      inc iyl");
      break;
      break;
    case 0x2d: // dec iyl
      iy.bytes.low=dec8(iy.bytes.low);
      DEBUGPRINT("      dec iyl");
      break;
    case 0x2e: // ld iyl,xx
      iy.bytes.low=fetch();
      DEBUGPRINT("  ld iyl,");
      DEBUGPHEX(iy.bytes.low);
      break;
    case 0x34: // inc (iy+xx)
      o=fetch();
      tempw=iy.word+o;
      tempb=readram(tempw);
      tempb=inc8(tempb);
      writeram(tempw,tempb);
      DEBUGPRINT("   inc (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x35: // dec (iy+xx)
      o=fetch();
      tempw=iy.word+o;
      tempb=readram(tempw);
      tempb=dec8(tempb);
      writeram(tempw,tempb);
      DEBUGPRINT("   dec (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x36: // ld (iy+xx),xx
      o=fetch();
      tempb=fetch();
      writeram(iy.word+o,tempb);
      DEBUGPRINT("ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),");
      DEBUGPHEX(tempb);
      break;
    case 0x39: // add iy,sp
      iy.word=add16(iy.word,spreg);
      DEBUGPRINT("      add iy,sp");
      break;
    case 0x44: // ld b,iyh
      bc.bytes.high=iy.bytes.high;
      DEBUGPRINT("      ld b,iyh");
      break;
    case 0x45: // ld b,iyl
      bc.bytes.high=iy.bytes.low;
      DEBUGPRINT("      ld b,iyl");
      break;
    case 0x46: // ld b,(iy+xx)
      o=fetch();
      bc.bytes.high=readram(iy.word+o);
      DEBUGPRINT("   ld b,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x4c: // ld c,iyh
      bc.bytes.low=iy.bytes.high;
      DEBUGPRINT("      ld c,iyh");
      break;
    case 0x4d: // ld c,iyl
      bc.bytes.low=iy.bytes.low;
      DEBUGPRINT("      ld c,iyl");
      break;
    case 0x4e: // ld c,(iy+xx)
      o=fetch();
      bc.bytes.low=readram(iy.word+o);
      DEBUGPRINT("   ld c,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x54: // ld d,iyh
      de.bytes.high=iy.bytes.high;
      DEBUGPRINT("      ld d,iyh");
      break;
    case 0x55: // ld d,iyl
      de.bytes.high=iy.bytes.low;
      DEBUGPRINT("      ld d,iyl");
      break;
    case 0x56: // ld d,(iy+xx)
      o=fetch();
      de.bytes.high=readram(iy.word+o);
      DEBUGPRINT("   ld d,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x5c: // ld e,iyh
      de.bytes.low=iy.bytes.high;
      DEBUGPRINT("      ld e,iyh");
      break;
    case 0x5d: // ld e,iyl
      de.bytes.low=iy.bytes.low;
      DEBUGPRINT("      ld e,iyl");
      break;
    case 0x5e: // ld e,(iy+xx)
      o=fetch();
      de.bytes.low=readram(iy.word+o);
      DEBUGPRINT("   ld e,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x60: // ld iyh,b
      iy.bytes.high=bc.bytes.high;
      DEBUGPRINT("      ld iyh,b");
      break;
    case 0x61: // ld iyh,c
      iy.bytes.high=bc.bytes.low;
      DEBUGPRINT("      ld iyh,c");
      break;
    case 0x62: // ld iyh,d
      iy.bytes.high=de.bytes.high;
      DEBUGPRINT("      ld iyh,d");
      break;
    case 0x63: // ld iyh,e
      iy.bytes.high=de.bytes.low;
      DEBUGPRINT("      ld iyh,e");
      break;
    case 0x64: // ld iyh,iyh
      DEBUGPRINT("      ld iyh,iyh");
      break;
    case 0x65: // ld iyh,iyl
      iy.bytes.high=iy.bytes.low;
      DEBUGPRINT("      ld iyh,iyl");
      break;
    case 0x66: // ld h,(iy+xx)
      o=fetch();
      hl.bytes.high=readram(iy.word+o);
      DEBUGPRINT("   ld h,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x67: // ld iyh,a
      iy.bytes.high=acc;
      DEBUGPRINT("      ld iyh,a");
      break;
    case 0x68: // ld iyl,b
      iy.bytes.low=bc.bytes.high;
      DEBUGPRINT("      ld iyl,b");
      break;
    case 0x69: // ld iyl,c
      iy.bytes.low=bc.bytes.low;
      DEBUGPRINT("      ld iyl,c");
      break;
    case 0x6a: // ld iyl,d
      iy.bytes.low=de.bytes.high;
      DEBUGPRINT("      ld iyl,d");
      break;
    case 0x6b: // ld iyl,e
      iy.bytes.low=de.bytes.low;
      DEBUGPRINT("      ld iyl,e");
      break;
    case 0x6c: // ld iyl,iyh
      iy.bytes.low=iy.bytes.high;
      DEBUGPRINT("      ld iyl,iyh");
      break;
    case 0x6d: // ld iyl,iyl
      DEBUGPRINT("      ld iyl,iyl");
      break;
    case 0x6e: // ld l,(iy+xx)
      o=fetch();
      hl.bytes.low=readram(iy.word+o);
      DEBUGPRINT("   ld l,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x6f: // ld iyl,a
      iy.bytes.low=acc;
      DEBUGPRINT("      ld iyl,a");
      break;
    case 0x70: // ld (iy+xx),b
      o=fetch();
      writeram(iy.word+o,bc.bytes.high);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),b");
      break;
    case 0x71: // ld (iy+xx),c
      o=fetch();
      writeram(iy.word+o,bc.bytes.low);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),c");
      break;
    case 0x72: // ld (iy+xx),d
      o=fetch();
      writeram(iy.word+o,de.bytes.high);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),d");
      break;
    case 0x73: // ld (iy+xx),e
      o=fetch();
      writeram(iy.word+o,de.bytes.low);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),e");
      break;
    case 0x74: // ld (iy+xx),h
      o=fetch();
      writeram(iy.word+o,hl.bytes.high);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),h");
      break;
    case 0x75: // ld (iy+xx),l
      o=fetch();
      writeram(iy.word+o,hl.bytes.low);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),l");
      break;
    case 0x77: // ld (iy+xx),a
      o=fetch();
      writeram(iy.word+o,acc);
      DEBUGPRINT("   ld (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT("),a");
      break;
    case 0x7c: // ld a,iyh
      acc=iy.bytes.high;
      DEBUGPRINT("      ld a,iyh");
      break;
    case 0x7d: // ld a,iyl
      acc=iy.bytes.low;
      DEBUGPRINT("      ld a,iyl");
      break;
    case 0x7e: // ld a,(iy+xx)
      o=fetch();
      acc=readram(iy.word+o);
      DEBUGPRINT("   ld a,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x7f:
      break;
    case 0x84: // add a,iyh
      acc=add8(acc,iy.bytes.high);
      DEBUGPRINT("      add a,iyh");
      break;
    case 0x85: // add a,iyl
      acc=add8(acc,iy.bytes.low);
      DEBUGPRINT("      add a,iyl");
      break;
    case 0x86: // add a,(iy+xx)
      o=fetch();
      acc=add8(acc,readram(iy.word+o));
      DEBUGPRINT("   add a,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x8c: // adc a,iyh
      acc=adc8(acc,iy.bytes.high);
      DEBUGPRINT("      adc a,iyh");
      break;
    case 0x8d: // adc a,iyl
      acc=adc8(acc,iy.bytes.low);
      DEBUGPRINT("      adc a,iyl");
      break;
    case 0x8e: // adc a,(iy+xx)
      o=fetch();
      acc=adc8(acc,readram(iy.word+o));
      DEBUGPRINT("   adc a,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x94: // sub iyh
      acc=sub8(acc,iy.bytes.high);
      DEBUGPRINT("      sub iyh");
      break;
    case 0x95: // sub iyl
      acc=sub8(acc,iy.bytes.low);
      DEBUGPRINT("      sub iyl");
      break;
    case 0x96: // sub (iy+xx)
      o=fetch();
      acc=sub8(acc,readram(iy.word+o));
      DEBUGPRINT("   sub (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x9c: // sbc a,iyh
      acc=sbc8(acc,iy.bytes.high);
      DEBUGPRINT("      sbc a,iyh");
      break;
    case 0x9d: // sbc a,iyl
      acc=sbc8(acc,iy.bytes.low);
      DEBUGPRINT("      sbc a,iyl");
      break;
    case 0x9e: // sbc a,(iy+xx)
      o=fetch();
      acc=sbc8(acc,readram(iy.word+o));
      DEBUGPRINT("   sbc a,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xa4: // and iyh
      acc&=iy.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      and iyh");
      break;
    case 0xa5: // and l
      acc&=iy.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("      and iyl");
      break;
    case 0xa6: // and (iy+xx)
      o=fetch();
      acc&=readram(iy.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG);
      setflags(HFLAG);
      DEBUGPRINT("   and (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xac: // xor iyh
      acc^=iy.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      xor iyh");
      break;
    case 0xad: // xor iyl
      acc^=iy.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      xor iyl");
      break;
    case 0xae: // xor (iy+xx)
      o=fetch();
      acc^=readram(iy.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("   xor (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xb4: // or iyh
      acc|=iy.bytes.high;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      or iyh");
      break;
    case 0xb5: // or iyl
      acc|=iy.bytes.low;
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("      or iyl");
      break;
    case 0xb6: // or (iy+xx)
      o=fetch();
      acc|=readram(iy.word+o);
      setlogicflags(acc);
      clearflags(CFLAG|NFLAG|HFLAG);
      DEBUGPRINT("   or (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xbc: // cp iyh
      sub8(acc,iy.bytes.high);
      DEBUGPRINT("      cp iyh");
      break;
    case 0xbd: // cp iyl
      sub8(acc,iy.bytes.low);
      DEBUGPRINT("      cp iyl");
      break;
    case 0xbe: // cp (iy+xx)
      o=fetch();
      sub8(acc,readram(iy.word+o));
      DEBUGPRINT("   cp (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xcb:
      emulate_fdcb(); // iy bits
      break;
    case 0xe1: // pop iy
      iy.word=popw();
      DEBUGPRINT("      pop iy");
      break;
    case 0xe3: // ex (sp),iy
      tempb=readram(spreg);
      writeram(spreg,iy.bytes.low);
      iy.bytes.low=tempb;
      tempb=readram(spreg+1);
      writeram(spreg+1,iy.bytes.high);
      iy.bytes.high=tempb;
      DEBUGPRINT("      ex (sp),iy");
      break;
    case 0xe5: // push iy
      pushw(iy.word);
      DEBUGPRINT("      push iy");
      break;
    case 0xe9: // jp (iy)
      pcreg=iy.word;
      DEBUGPRINT("      jp (iy)");
      break;
    case 0xf9: // ld sp,iy
      spreg=iy.word;
      DEBUGPRINT("      ld sp,iy");
      break;
    default:
      //ERRORPRINT("invalid instruction FD ");
      //ERRORPHEX(tempb);
      //ERRORPRINT(" at ");
      //ERRORPHEX16(pcreg-2);
      return false;
  }
  return true;
}


void z80::emulate_fdcb()
{
uint8_t b,v,o;
uint16_t w;
  o=fetch();
  b=fetch();
  w=o+iy.word; // all instructions take the index, even undocumented
  v=readram(w);
  switch (b) {
    case 0x06: // rlc (iy+xx)
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v<<=1;
      v|=carryflag();
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rlc (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x0e: // rrc (iy+xx)
      clearflags(CFLAG|NFLAG|HFLAG);
      if (v&1)
        setflags(CFLAG);
      v>>=1;
      if (testflag(CFLAG))
        v|=0x80;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rrc (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x16: // rl (iy+xx)
      clearflags(NFLAG|HFLAG);
      b=v;
      v=(v<<1)|carryflag();
      if (b&0x80)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rl (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x1e: // rr (iy+xx)
      clearflags(NFLAG|HFLAG);
      b=v;
      v=(v>>1);
      if (testflag(CFLAG))
        v|=0x80;
      if (b&0x01)
        setflags(CFLAG);
      else
        clearflags(CFLAG);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("rr (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x26: // sla (iy+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=v<<1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sla (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x2e: // sra (iy+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=(v&0x80)|(v>>1);
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sra (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x36: // sll (iy+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x80)
        setflags(CFLAG);
      v=(v<<1)|1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("sll (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x3e: // srl (iy+xx)
      clearflags(NFLAG|HFLAG|CFLAG);
      if (v&0x01)
        setflags(CFLAG);
      v=v>>1;
      setlogicflags(v);
      writeram(w,v);
      DEBUGPRINT("srl (iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;      
    case 0x46: // bit 0,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x01))
        setflags(ZFLAG);
      DEBUGPRINT("bit 0,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x4e: // bit 1,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x02))
        setflags(ZFLAG);
      DEBUGPRINT("bit 1,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x56: // bit 2,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x04))
        setflags(ZFLAG);
      DEBUGPRINT("bit 2,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x5e: // bit 3,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x08))
        setflags(ZFLAG);
      DEBUGPRINT("bit 3,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x66: // bit 4,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x10))
        setflags(ZFLAG);
      DEBUGPRINT("bit 4,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x6e: // bit 5,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x20))
        setflags(ZFLAG);
      DEBUGPRINT("bit 5,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x76: // bit 6,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x40))
        setflags(ZFLAG);
      DEBUGPRINT("bit 6,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x7e: // bit 7,(iy+xx)
      clearflags(ZFLAG|NFLAG);
      setflags(HFLAG);
      if (!(v&0x80))
        setflags(ZFLAG);
      DEBUGPRINT("bit 7,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x86: // res 0,(iy+xx)
      v&=~(0x01);
      writeram(w,v);
      DEBUGPRINT("res 0,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x8e: // res 1,(iy+xx)
      v&=~(0x02);
      writeram(w,v);
      DEBUGPRINT("res 1,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x96: // res 2,(iy+xx)
      v&=~(0x04);
      writeram(w,v);
      DEBUGPRINT("res 2,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0x9e: // res 3,(iy+xx)
      v&=~(0x08);
      writeram(w,v);
      DEBUGPRINT("res 3,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xa6: // res 4,(iy+xx)
      v&=~(0x10);
      writeram(w,v);
      DEBUGPRINT("res 4,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xae: // res 5,(iy+xx)
      v&=~(0x20);
      writeram(w,v);
      DEBUGPRINT("res 5,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xb6: // res 6,(iy+xx)
      v&=~(0x40);
      writeram(w,v);
      DEBUGPRINT("res 6,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xbe: // res 7,(iy+xx)
      v&=~(0x80);
      writeram(w,v);
      DEBUGPRINT("res 7,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xc6: // set 0,(iy+xx)
      v|=0x01;
      writeram(w,v);
      DEBUGPRINT("set 0,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xce: // set 1,(iy+xx)
      v|=0x02;
      writeram(w,v);
      DEBUGPRINT("set 1,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xd6: // set 2,(iy+xx)
      v|=0x04;
      writeram(w,v);
      DEBUGPRINT("set 2,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xde: // set 3,(iy+xx)
      v|=0x08;
      writeram(w,v);
      DEBUGPRINT("set 3,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xe6: // set 4,(iy+xx)
      v|=0x10;
      writeram(w,v);
      DEBUGPRINT("set 4,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xee: // set 5,(iy+xx)
      v|=0x20;
      writeram(w,v);
      DEBUGPRINT("set 5,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xf6: // set 6,(iy+xx)
      v|=0x40;
      writeram(w,v);
      DEBUGPRINT("set 6,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    case 0xfe: // set 7,(iy+xx)
      v|=0x80;
      writeram(w,v);
      DEBUGPRINT("set 7,(iy+");
      DEBUGPHEX(o);
      DEBUGPRINT(")");
      break;
    default:
      ERRORPRINT("invalid instruction FD CB ");
      ERRORPHEX(o);
      ERRORPRINT(" ");
      ERRORPHEX(b);
      ERRORPRINT(" at ");
      ERRORPHEX16(pcreg-3);
      fault();
      break;
  }
}
