


# create a lookup table for zero, sign, and parity flag based on 8 bit value

import string

def isevenparity(v):
  bits=0
  while (v):
    if v&1:
      bits+=1
    v=v>>1
  return bits&1==0

for v in range(0,256):
  r=["0"]
  if v==0:
    r.append("ZFLAG")
  if v>127:
    r.append("SFLAG")
  if isevenparity(v):
    r.append("PVFLAG")
  print "  %s, //%d"%(string.join(r,"|"),v)
