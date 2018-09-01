# z-two
CP/M machine with Z80 emulated on ATMega

See project page at http://www.nomad.ee/micros/z-two/ for full story.

Z-Two runs about 110000 Z80 instructions per second on emulator
running on ATMega1284p, and is fully functional CP/M machine,
albeit a slow one. It uses SD card as storage (maxed out disk
space for CP/M, 16x8MB drives), VGA monitor (80x30 text)
and PS/2 keyboard. There is a serial port with USB-Serial
converter for getting data in and out of the machine.

The emulation code can also be built to run emulated version of
the machine on linux or OSX where the SD card is emulated on a
regular file.

Z80 emulation code was written from scratch, and has features for
debugging and profiling the emulator. It runs all documented Z80
instructions, and enough undocumented ones to pass all 
Frank Cringle's yaze tests (documented flags only).
