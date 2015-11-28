#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

void error(int code, char* obj, char* msg) {
  printf("%s: %s\n", obj, msg);
  exit(code);
}

void dump_buf_to_file(char* filename, BYTE *buf, long length) {

  FILE *f = fopen(filename, "w");
  long left = length;
  long offset = 0;

  while (left != 0) {
    long writ = fwrite(buf+offset, sizeof(BYTE), left, f);
    left -= writ;
    offset += writ;
  }

  fclose(f);
  printf("Writ %s\n",filename );
}

void print_int_bytes(BYTE *buf, int x) {
  for (int i=0;i<x;i++)
    printf("%d ", buf[i]);
}

void print_chr_bytes(BYTE *buf, int x) {
  for (int i=0;i<x;i++)
    putchar(buf[i]);
}
