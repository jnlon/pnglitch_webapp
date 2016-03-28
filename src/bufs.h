#ifndef BUFS_H_
#define BUFS_H_

typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

void append_bytes(unsigned char *basebuf, unsigned char *inbuf, long long *base_offset, int inbuf_length);
void buf_slice(long start, long len, unsigned char *result, unsigned char *data);
void int_to_four_bytes(uint i, unsigned char *buf);
unsigned int four_bytes_to_int(unsigned char bb[]);
void buf_read(unsigned char *dst, unsigned char **src, int to_read);

#endif
