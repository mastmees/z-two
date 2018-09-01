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
#ifndef __z80_flags_included__
#define __z80_flags_included__

#ifdef __AVR_ARCH__
#include <avr/pgmspace.h>
#endif
#include <stdint.h>

// bitmasks for flags in F register
#define SFLAG  0x80
#define ZFLAG  0x40
#define F5FLAG 0x20
#define HFLAG  0x10
#define F3FLAG 0x08
#define PVFLAG 0x04
#define NFLAG  0x02
#define CFLAG  0x01

#define carryflag() (flags&1)
#define clearflags(b) flags&=~(b)
#define setflags(b) flags|=b
#define flipflags(b) flags^=b
#define testflag(b) (flags&b)

extern const uint8_t z80_logicflags[256];

#endif
