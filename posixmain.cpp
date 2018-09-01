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
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "z80.hpp"
#include "console.hpp"
#include "aux.hpp"
#include "partitioner.hpp"
#include "machine.hpp"
#include "sdcard.hpp"

z80 cpu;
SDCard sdcard;

int main(int argc,char *argv[])
{
bool forcemonitor=false;
FILE *fp;
uint8_t c;
int adr=0x100; // CPM program area start
  initialize_machine();
  for (int i=1;i<argc;i++) {
    if (!strcmp(argv[i],"-m"))
      forcemonitor=true;
    if (!strcmp(argv[i],"-l")) { // load program into ram
      i++;
      fp=fopen(argv[i],"rb");
      if (fp) {
        while ((adr<=0xffff) && !ferror(fp) && !(feof(fp))) {
          fread(&c,1,1,fp);
          cpu.writeram(adr++,c);
        }
        fclose(fp);
        printf("loaded %s, %d pages\r\n",argv[i],(adr+255)>>8);
        sleep(1);
      }
    }
  }
  if (!forcemonitor)
    sdcard.Init();
  console.init();
  aux.init();
  console.print("\ec\x0f\e[H\e[2JZ80 emulator for 1.0\r\n");
  checkdisk();
  copy_bootloader();
  cpu.reset();
  while (1) {
    cpu.step(10000); // running z80 instructions in batches reduces overhead
  } 
}

