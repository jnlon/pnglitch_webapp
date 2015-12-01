#ifndef DEBUG_H_
#define DEBUG_H_

#include "common.h"

void error(int code, char* obj, char* msg); 
void error_fatal(int code, char* obj, char* msg); 
void dump_buf_to_file(char* filename, BYTE *buf, long length);
void print_int_bytes(BYTE *buf, int x);
void print_chr_bytes(BYTE *buf, int x);

#endif
