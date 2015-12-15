#include "debug.h"
#include <stdio.h>
#include <stdlib.h>

void error_html(char* msg) {

}

void error_fatal(int code, char* obj, char* msg) {
  fprintf(stderr,"%s: %s\n", obj, msg);
  exit(code);
}

void error(int code, char* obj, char* msg) {
  fprintf(stderr,"%s: %s\n", obj, msg);
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
#ifdef DEBUG
  for (int i=0;i<x;i++)
    printf("%d ", buf[i]);
#endif
}

void print_chr_bytes(BYTE *buf, int x) {
#ifdef DEBUG
  for (int i=0;i<x;i++)
    putchar(buf[i]);
#endif
}
