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


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "z80.hpp"
#include "console.hpp"
#include "machine.hpp"
#include "sdcard.hpp"
#include "partitioner.hpp"
#include "aux.hpp"
#include "sdcard.hpp"

SDCard sdcard;
z80 cpu;

ISR(WDT_vect)
{
}

/*
I/O configuration
-----------------
I/O pin                               direction    DDR  PORT
PA0 D0                                input        0    1
PA1 D1                                input        0    1
PA2 D2                                input        0    1
PA3 D3                                input        0    1
PA4 D4                                input        0    1
PA5 D5                                input        0    1
PA6 D6                                input        0    1
PA7 D7                                input        0    1

PB0 A16                               output       1    0
PB1 A17                               output       1    0
PB2 A18                               output       1    0
PB3 /IORQ                             output       1    1
PB4 /SS                               output       1    1
PB5 MOSI                              output       1    1
PB6 MISO                              input        0    1
PB7 SCK                               output       1    1

PC0 A0                                output       1    0
PC1 A1                                output       1    0
PC2 A2                                output       1    0
PC3 A3                                output       1    0
PC4 A4                                output       1    0
PC5 A5                                output       1    0
PC6 A6                                output       1    0
PC7 A7                                output       1    0

PD0 RxD0                              input        0    1
PD1 TxD0                              output       1    1
PD2 RxD1                              input        0    1
PD3 TxD1                              output       1    1
PD4 /ALE                              output       1    1
PD5 /MREQ                             output       1    1
PD6 /RD                               output       1    1
PD7 /WR                               output       1    1
*/
int main(void)
{
  MCUSR=0;
  MCUCR=(1<<JTD); // this does not actually seem to help, fuse needs to be blown too

  // I/O directions and initial state
  DDRA=0x00;
  PORTA=0xff;
  DDRB=0xbf;
  PORTB=0xf8;
  DDRC=0xff;
  PORTC=0x00;
  DDRD=0xfa;
  PORTD=0xff;
  //
  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  // configure watchdog to interrupt&reset, 4 sec timeout
  WDTCSR|=0x18;
  WDTCSR=0xe8;
  // configure timer2 for generic timekeeping, triggering
  // overflow interrupts every 10ms
  TCCR2A=0;
  TCCR2B=7;
  TCNT2=255-180;
  TIMSK2=1;
  sei();
  console.init();
  aux.init();
  console.print("\ec\x0f\e[H\e[2JZ80 emulator for AVR 1.0\r\n");
  sdcard.Init();
  checkdisk();
  copy_bootloader();
  cpu.reset();
  while (1) {
    cpu.step(10000); // running z80 instructions in batches reduces overhead
    wdt_reset();
    WDTCSR|=0x40;
  } 
}

