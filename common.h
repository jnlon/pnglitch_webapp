#define MAX_PNG_IN_BYTESIZE 10737418240 //10MB
#define MAX_PNG_OUT_BYTESIZE 32212254720 //30MB
#define IN_BUF_SIZE 1024

#ifndef COMMON_H_
#define COMMON_H_

typedef unsigned char BYTE;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

//Need to be global for custom libpng write/read fns
extern long long MY_PNG_READ_OFFSET;
extern BYTE *ENTIRE_PNG_BUF;
extern long long PNG_LENGTH; //Also used for writing

void append_bytes(BYTE *basebuf, BYTE *inbuf, int base_offset, int inbuf_length);
void get_x_bytes(long start, long len, BYTE* result, BYTE* data);
void intTo4Bytes(uint i, BYTE *buf);
uint _4bytesToInt(BYTE *bb);

/*void append_bytes(BYTE *basebuf, BYTE *inbuf, int base_offset, int inbuf_length);
void get_x_bytes(long start, long len, BYTE* result, BYTE* data);
void intTo4Bytes(uint i, BYTE *buf);
uint _4bytesToInt(BYTE bb[]);*/


#endif
