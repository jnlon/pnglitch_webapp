#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void get_x_bytes(long start, long len, BYTE* result, BYTE* data)  {
  memcpy(result, &data[start], len);
}

void intTo4Bytes(uint i, BYTE *buf) {
  buf[0] = ((i >> 24) & 0xFF);
  buf[1] = ((i >> 16) & 0xFF);
  buf[2] = ((i >> 8) & 0xFF);
  buf[3] = (i & 0xFF);
}

uint _4bytesToInt(BYTE bb[]) {
  uint b1 = bb[0] & 0x0000FF;
  uint b2 = bb[1] & 0x0000FF;
  uint b3 = bb[2] & 0x0000FF;
  uint b4 = bb[3] & 0x0000FF;

  return (uint)((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
}

void append_bytes(BYTE *basebuf, BYTE *inbuf, int base_offset, int inbuf_length) {
  BYTE* end_of_buf = basebuf + base_offset;
  for (int i=0;i<inbuf_length;i++) {
    end_of_buf[i] = inbuf[i];
  }
}



