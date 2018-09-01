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
#ifndef __queue_hpp__
#define __queue_hpp__
#ifdef __AVR_ARCH__ 
#include <avr/io.h>
#endif
#include <stdio.h>
#include <stdint.h>

// queue size must be power of two (2,4,8,...)

template <class T,uint8_t S=16> class Queue
{
  T q[S];
  volatile uint8_t head,tail,count,size,mask;
public:
  Queue() : head(0),tail(0),count(0),size(S),mask(S-1)
  {
  }
  
  void Purge()    { count=0; head=tail; }
  uint8_t Count() { return count; }
  bool IsFull()   { return count==size; }

  void Push(T c)
  {
    if (!IsFull()) {
      q[head++]=c;
      head=head&mask;
      count++;
    }
  }
  
  T Pop()
  {
    T c=0;
    if (count) {
      c=q[tail++];
      tail=tail&mask;
      count--;
    }
    return c;
  }  
};
#endif
