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
#include "console.hpp"

#define isdigit(c) (c>='0' && c<='9')

Console console;

// terminal input straight off https://stackoverflow.com/questions/448944/c-non-blocking-keyboard-input
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
void Console::init()
{
}

bool Console::rxready()
{
  return rxqueue.Count()>0;
}  
  
uint8_t Console::receive()
{
  while (!rxready()) {
  }
  // blocks caller until data is received
  return rxqueue.Pop();
}

bool Console::txempty()
{
  return txqueue.Count()==0;
}

bool Console::txfull()
{
  return txqueue.IsFull();
}

void Console::send(uint8_t c)  
{
  printf("%c",c);
  fflush(stdout);
}

void Console::print(const char *s)
{
  while (s && *s) {
    send(*s++);
  }
}

void Console::print(int32_t n)
{
    if (n<0) {
      send('-');
      n=0-n;
    }
    if (n>9)
      print(n/10);
    send((n%10)+'0');
}

void Console::pxdigit(uint8_t c)
{
  c=(c&0x0f)+'0';
  if (c>'9')
    c+=7;
  send(c);
}

void Console::phex(uint8_t c)
{
  pxdigit(c>>4);
  pxdigit(c);
}

