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
#ifndef __partitioner_hpp__
#define __partitioner_hpp__

#include "sdcard.hpp"
extern SDCard sdcard;

typedef struct {
  uint8_t status;
  uint8_t firstchs[3];
  uint8_t type;
  uint8_t lastchs[3];
  uint8_t firstlba[4];
  uint8_t lbacount[4];
} PARTITION;

typedef struct {
  uint8_t status; // 0-16 user number, 0xe5 unused
  uint8_t filename[8];
  uint8_t extension[8];
  uint8_t xl; // extent number low bits
  uint8_t bc;
  uint8_t xh; // extent number high bits
  uint8_t rc; // records used in last used extent
  uint8_t allocations; // block pointers
} DIRENTRY;

void createpartitionentry(PARTITION* p,uint8_t type,uint32_t firstlba,uint32_t count);
void initialize_directory_sectors(uint32_t firstsector,uint16_t count);
void checkdisk(void);

#endif
