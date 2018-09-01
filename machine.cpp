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
#include "machine.hpp"
#include "sdcard.hpp"

/*
this module implements the physical machine around z80 emulator, providing access
to RAM and I/O space, and implementing some of the I/O using AVR hardware - 
serial ports, SD card inteface, and timer.
*/


enum MSTATE { IDLE, BIOS, CPM, TIMECOUNTER };

static uint32_t timecounter,timecountersnapshot; // counts 10ms ticks
static uint8_t auxbaud;
static uint8_t sd0,sd1,sd2,sd3,sds;
static uint8_t sdc;
static uint16_t dataofs;
static MSTATE state;
static uint16_t count;

static uint16_t baudrates[8]= {
 50, 300,1200,2400,4800,9600,19200,38400 
};

// prototypes for functions that handle internal I/O space access
static uint8_t io_read(uint16_t adr);
static void io_write(uint16_t adr,uint8_t b);

ISR(TIMER2_OVF_vect)
{
  TCNT2=255-180;
  timecounter++;
  console.tick();
}

#define mreq_low() PORTD&=0xdf
#define mreq_high() PORTD|=0x20
#define wr_low() PORTD&=0x7f
#define wr_high() PORTD|=0x80
#define ale_low() PORTD&=0xef
#define ale_high() PORTD|=0x10
#define rd_low() PORTD&=0xbf
#define rd_high() PORTD|=0x40
#define iorq_low() PORTB&=0xf7
#define iorq_high() PORTB|=0x08

// read one byte of data from memory address
// Z80 only directly addresses 64K, any paging method for larger RAM
// needs to be applied here
uint8_t z80::readram(uint16_t adr)
{
uint8_t b;
  PORTC=adr>>8;
  ale_low();
  ale_high();
  PORTC=adr&255;
  mreq_low();
  rd_low();
  PORTA=0xff; // have to delay one instruction here, so do something useful and enable pullups
  b=PINA;
  rd_high();
  mreq_high();
  return b; 
}

// write one byte of data to memory address
// Z80 only directly addresses 64K, any paging method for larger RAM
// needs to be applied here
void z80::writeram(uint16_t adr,uint8_t data)
{
  PORTC=adr>>8;
  ale_low();
  ale_high();
  PORTC=adr&255;
  DDRA=0xff;
  PORTA=data;
  mreq_low();
  wr_low();
  wr_high();
  mreq_high();
  DDRA=0x00;
}

// read data from I/O space.
// real Z80 actually sets the high 8 bits of address, but
// current emulator only uses low 8
uint8_t z80::readio(uint16_t adr)
{
uint8_t b;
  if (adr>=0xa0)
    b=io_read(adr);
  else {
    PORTC=adr>>8;
    ale_low();
    ale_high();
    PORTC=adr&255;
    iorq_low();
    rd_low();
    PORTA=0xff;
    b=PINA;
    rd_high();
    iorq_high();
  }
  return b; 
}

// write data to I/O space.
// real Z80 actually sets the high 8 bits of address, but
// current emulator only uses low 8
void z80::writeio(uint16_t adr,uint8_t data)
{ 
  if (adr>=0xa0)
    io_write(adr,data);
  else {
    PORTC=adr>>8;
    ale_low();
    ale_high();
    PORTC=adr&255;
    DDRA=0xff;
    PORTA=data;
    iorq_low();
    wr_low();
    wr_high();
    iorq_high();
    DDRA=0;
  }
}

// this is called when invalid instruction is encountered
void z80::fault(void)
{
  console.print("\r\nZ80 emulator fault. PC=");
  console.phex(pcreg>>8);
  console.phex(pcreg&255);
  while (1); 
}

void copy_bootloader(void)
{
uint16_t a;
uint8_t b;
  console.print("Bootstrapping...\r\n");
  for (a=0;a<sizeof(bootstrap_bin);a++) {
    b=pgm_read_byte(&bootstrap_bin[a]);
    cpu.writeram(a,b);
  }  
}


static uint8_t io_read(uint16_t adr)
{
uint8_t b=0;
  switch (adr) {

    // misc operations data read
    
    case 0xa1: 

      switch (state) {
        case BIOS: // reading bios code
          if (count>=sizeof(monitor_bin))
            b=0x55;
          else {
            b=pgm_read_byte(&monitor_bin[count]);
            count++;
          }
          break;
        case CPM:
          if (count>=sizeof(cpm_bin))
            b=0x55;
          else {
            b=pgm_read_byte(&cpm_bin[count]);
            count++;
          }
          break;
        case TIMECOUNTER:
          b=timecountersnapshot&255;
          timecountersnapshot>>=8;
          break;
        default:
          state=IDLE;
          break;
      }
      break;

    // console input / status
    
    case 0xa2: // console data
      if (console.rxready()) {
        b=console.receive();
      }
      break;
    case 0xa3: // console status
      b=console.rxready()?1:0;
      b|=console.txfull()?2:0;
      break;
    
    // aux input/status
    
    case 0xa4: // aux data
      b=aux.receive();
      break;
    case 0xa5: // aux status
      b|=aux.rxcount()?0x40:0;
      b|=aux.txcount()?0x20:0;
      b|=aux.txfull()?0x10:0;
      b|=auxbaud;
      break;
      
    // sd card status/read
    
    case 0xa8: // A_SDC - read back SD card command
      b=sdc;
      break;
    case 0xa9: // A_SDD - read SD card data
      if ((sdc&0xc0)==0x80) { // still reading sector data
        b=sdcard.GetBuf()[dataofs];
        dataofs++;
        if (dataofs>511)
          sdc|=0x40;
      }
      break;
    case 0xaa: // A_SDS - read SD card status
      b=sds;
      break;
    case 0xab:
      b=sd0;
      break;
    case 0xac:
      b=sd1;
      break;
    case 0xad:
      b=sd2;
      break;
    case 0xae:
      b=sd3;
      break;
  }
  return b;
}

static void io_write(uint16_t adr,uint8_t b)
{
uint32_t s;
#ifdef INSTRUCTIONCOUNTER
uint32_t total;
#endif
#ifdef INSTRUCTIONPROFILER
uint32_t v32;
uint16_t i;
uint8_t c;
#endif
  switch (adr) {

    // misc commands
    
    case 0xa0: // misc command port
      switch (b) {
        case 0: // load bios
          state=BIOS;
          count=0;
          break;
        case 1: // reboot
          copy_bootloader();
          cpu.reset();
          state=IDLE;
          break;
        case 2: // load CP/M
          state=CPM;
          count=0;
          break;
        case 3: // reset timecounter
          timecounter=0;
          break;
        case 4: // read timecounter
          state=TIMECOUNTER;
          cli();
          timecountersnapshot=timecounter;
          sei();
          break;
        case 5: // dump instruction profiler data
          #ifdef INSTRUCTIONCOUNTER
          cli();
          timecountersnapshot=timecounter;
          sei();
          #endif
          #ifdef INSTRUCTIONPROFILER
          console.print("\r\n");
          c=0;
          for (i=0;i<256;i++)
          {
            v32=profilercounts[i];
            if (v32) {
              c++;
              console.phex(i);
              console.print(" ");
              console.phex((v32>>16)&255);
              console.phex16(v32&0xffff);
              //console.print(v32);
              if (c>7) {
                console.print("\r\n");
                c=0;
              }
              else
                console.print(" ");
            }
          }
          #endif
          #ifdef INSTRUCTIONCOUNTER
          console.print("\r\n");
          console.print(profilecounter);
          console.print(" instructions in ");
          console.print(timecountersnapshot);
          console.print(" ticks (");
          total=profilecounter/(timecountersnapshot/100);
          console.print(total);
          console.print(" IPS)\r\n");
          #endif
          break;          
        default:
          state=IDLE;
          break;
      }
      break;

    // console output

    case 0xa2: // console data
      console.send(b);
      break;

    // aux output/control

    case 0xa4: // aux data
      aux.send(b);
      break;
    case 0xa5: // aux control
      auxbaud=b&0x87;
      if (b&0x80) {
        aux.setspeed(baudrates[auxbaud&7]);
      }
      else {
        aux.setspeed(115200); // if aux 'disabled', then reset back to 115200
      }
      break;
      
    // SD card command / write

    case 0xa8: // SDC - sdcard command
      sdc=b;
      switch (b) {
        case 0: // read
          sds=sdcard.ReadSector((uint32_t)sd3<<24|(uint32_t)sd2<<16|(uint32_t)sd1<<8|sd0,
            sdcard.GetBuf());
          dataofs=0;
          sdc|=0x80;
          break;
        case 1: // write, this will need data from data register first
          dataofs=0;
          break;
        case 2: // get card type
          sds=sdcard.GetType();          
          sdc|=0x80;
          break;
        case 3: // get card size
          s=sdcard.GetTotalSectors();
          sd0=s;
          sd1=s>>8;
          sd2=s>>16;
          sd3=s>>24;
          sdc|=0x80;
          break;
        case 4: // reset
          sds=sdcard.Init(true); // silent initialize
          sdc|=0x80;
          break;
        default: // unknowns
          sds=0xff;
          sdc|=0x80;
          break;
      }
      break;
    case 0xa9: // SDD
      if (sdc==1 && dataofs<512) {
        sdcard.GetBuf()[dataofs++]=b;
        if (dataofs>511) {
          sds=sdcard.WriteSector((uint32_t)sd3<<24|(uint32_t)sd2<<16|(uint32_t)sd1<<8|sd0,
             sdcard.GetBuf());
          sdc|=0x80;
        }
      }
      break;
    case 0xab:
      sd0=b;
      break;
    case 0xac:
      sd1=b;
      break;
    case 0xad:
      sd2=b;
      break;
    case 0xae:
      sd3=b;
      break;
  }
}

