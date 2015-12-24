#ifndef BUFS_H_
#define BUFS_H_

typedef unsigned char BYTE;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

void append_bytes(BYTE *basebuf, BYTE *inbuf, long long *base_offset, int inbuf_length);
void buf_slice(long start, long len, BYTE* result, BYTE* data);
void int_to_four_bytes(uint i, BYTE *buf);
unsigned int four_bytes_to_int(BYTE bb[]);
void buf_read(unsigned char *dst, unsigned char **src, int to_read);

#endif
