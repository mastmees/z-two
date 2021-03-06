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
#include "aux.hpp"

#define isdigit(c) (c>='0' && c<='9')

#define BAUDRATE 115200L

Aux aux;

// low-level serial port tx and rx queues

ISR(USART1_RX_vect) // Rx complete
{
  aux.rxqueue.Push(UDR1);
}

ISR(USART1_UDRE_vect) // Data register empty
{
  if (aux.txqueue.Count())
    UDR1=aux.txqueue.Pop();
  else { // disable tx interrupts if no data
    UCSR1B&=~(1<<UDRIE0);
  }
}

void Aux::setspeed(uint32_t speed)
{
uint16_t ubrr;
  ubrr=(F_CPU/(16L*speed)-1);
  UBRR1H=(uint8_t)(ubrr>>8);
  UBRR1L=(uint8_t)ubrr;
}

void Aux::init()
{
  setspeed(BAUDRATE);
  UCSR1A=0;
  UCSR1B=(1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
  UCSR1C=(1<<USBS0)|(3<<UCSZ00);
}

bool Aux::rxready()
{
  return rxqueue.Count()!=0;
}  
  
uint8_t Aux::receive()
{
  while (!rxready()) {
    wdt_reset();
    WDTCSR|=0x40;
  }
  // blocks caller until data is received
  return rxqueue.Pop();
}

bool Aux::txempty()
{
  return txqueue.Count()==0;
}

bool Aux::txfull()
{
  return txqueue.IsFull();
}

void Aux::send(uint8_t c)  
{
  while (txfull())
  {
    wdt_reset();
    WDTCSR|=0x40;
  }
  cli();
  txqueue.Push(c);
  UCSR1B|=(1<<UDRIE0);
  sei();
}

void Aux::print(const char *s)
{
  while (s && *s) {
    send(*s++);
  }
}

void Aux::print(int32_t n)
{
    if (n<0) {
      send('-');
      n=0-n;
    }
    if (n>9)
      print(n/10);
    send((n%10)+'0');
}

void Aux::pxdigit(uint8_t c)
{
  c=(c&0x0f)+'0';
  if (c>'9')
    c+=7;
  send(c);
}

void Aux::phex(uint8_t c)
{
  pxdigit(c>>4);
  pxdigit(c);
}

