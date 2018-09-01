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

#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <termios.h>
#if !__linux__
#include <pthread.h>
#endif

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

uint8_t ramspace[65536L];
uint8_t iospace[256];

static uint8_t io_read(uint16_t adr);
static void io_write(uint16_t adr,uint8_t b);

struct termios orig_termios;

void reset_terminal_mode()
{
    tcsetattr(0, TCSANOW, &orig_termios);
}

void set_conio_terminal_mode()
{
    struct termios new_termios;

    /* take two copies - one for now, one for later */
    tcgetattr(0, &orig_termios);
    memcpy(&new_termios, &orig_termios, sizeof(new_termios));

    /* register cleanup handler, and set the new terminal mode */
    atexit(reset_terminal_mode);
    cfmakeraw(&new_termios);
    new_termios.c_cc[VERASE]=8;
    tcsetattr(0, TCSANOW, &new_termios);
}

int kbhit()
{
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv)>0;
}


int getch()
{
    int r;
    unsigned char c;
    if ((r = read(0, &c, sizeof(c))) < 0) {
        return r;
    } else {
      if (c==0x7f)
        c=8;
      return c;
    }
}

static void timerhandler(int sig,siginfo_t *si,void *ucontext)
{
  timecounter++;
  console.tick();
  if (kbhit())
    console.rxqueue.Push(getch());
}


#if __linux__
#else
static void * timer_thread(void *arg)
{
struct timespec sleeptime;
  sleeptime.tv_sec=0;
  sleeptime.tv_nsec=10000000L;
  while (1) {
    nanosleep(&sleeptime,NULL);
    timerhandler(0,NULL,NULL);
  }
}

#endif

void initialize_machine(void)
{
#if __linux__
timer_t t;
struct itimerspec timerinterval;
struct sigaction action;
struct sigevent sev;
  action.sa_sigaction=timerhandler;
  sigemptyset(&action.sa_mask);
  action.sa_flags=SA_SIGINFO;
  if (sigaction(SIGALRM,&action,NULL)) {
    perror("sigaction");
    return;
  }
  sev.sigev_notify=SIGEV_SIGNAL;
  sev.sigev_signo=SIGALRM;
  sev.sigev_value.sival_ptr=&t;
  if (!timer_create(CLOCK_MONOTONIC,&sev,&t)) {
   // 10 ms intervals, starting 10 ms from now
    timerinterval.it_value.tv_sec=0;
    timerinterval.it_value.tv_nsec=10000000L;
    timerinterval.it_interval.tv_sec=0;
    timerinterval.it_interval.tv_nsec=10000000L;
    if (timer_settime(t,0,&timerinterval,NULL))
      perror("timer_create");
  }
#else
pthread_t thread;
  pthread_create(&thread,NULL,timer_thread,NULL);
#endif
  set_conio_terminal_mode();
}

// read one byte of data from memory address
// Z80 only directly addresses 64K, any paging method for larger RAM
// needs to be applied here
uint8_t z80::readram(uint16_t adr)
{
  return ramspace[adr];
}

// write one byte of data to memory address
// Z80 only directly addresses 64K, any paging method for larger RAM
// needs to be applied here
void z80::writeram(uint16_t adr,uint8_t data)
{
  ramspace[adr]=data;
}

// read data from I/O space.
// real Z80 actually sets the high 8 bits of address, but
// current emulator only uses low 8
uint8_t z80::readio(uint16_t adr)
{
  if (adr>=0xa0)
    return io_read(adr);
  return iospace[adr];
}

// write data to I/O space.
// real Z80 actually sets the high 8 bits of address, but
// current emulator only uses low 8
void z80::writeio(uint16_t adr,uint8_t data)
{ 
  if (adr>=0xa0)
    io_write(adr,data);
  else
    iospace[adr]=data;
}

// this is called when invalid instruction is encountered
void z80::fault(void)
{
  console.print("\r\n");
  //console.print("Z80 emulator fault. PC=");
  //console.phex(pcreg>>8);
  //console.phex(pcreg&255);
}

void copy_bootloader(void)
{
uint16_t a;
uint8_t b;
  console.print("Bootstrapping...\r\n");
  for (a=0;a<sizeof(bootstrap_bin);a++) {
    b=bootstrap_bin[a];
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
            b=monitor_bin[count];
            count++;
          }
          break;
        case CPM:
          if (count>=sizeof(cpm_bin))
            b=0x55;
          else {
            b=cpm_bin[count];
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
          timecountersnapshot=timecounter;
          break;
        case 5: // dump instruction profiler data
          #ifdef INSTRUCTIONCOUNTER
          timecountersnapshot=timecounter;
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
          printf("\r\n%llu instructions in %u ticks (%u IPS)\r\n",
            profilecounter,timecountersnapshot,(uint32_t)(profilecounter/(timecountersnapshot/100)));
          #endif
          break;          
        case 6:
          exit(0);
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

