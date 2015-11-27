#include <png.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <unistd.h>

#define MAX_PNG_IN_BYTESIZE 10737418240 //10MB
#define MAX_PNG_OUT_BYTESIZE 32212254720 //30MB
#define IN_BUF_SIZE 1024

/*
 * As far as I can tell, libpng offers no way of accessing the filter type on a given row,
 * which is sad because it gives you pixel data THAT IS ALREADY FILTERED. 
 *
 *Plan B: 
 * Use Libpng to tansform the input into into RGB format 
 * (basically re-encode image using all filter methods)
 * Using the write-callback method, store this data in a shared buffer.
 * Then, glitch this buffer to hell java-pnglitch-style
 *
 * Step 1: Basically, port pnglitch-java to c (hello zlib!)
 * Step 2: Use libpng's transform functions to make 
 *         image the most glitchable, save this to buffer
 * Step 3: Run pnglitch-port on buffer
 *
 *
 * TODO: 
 * How to give a raw buffer to libpng as an image?
 *  -> Use this to handle getting image size / etc
 *  -> OFC conversion stuff too
 *
 */

typedef unsigned char BYTE;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef unsigned long long ulonglong;

BYTE IDAT_HDR_BYTES[] = {73, 68, 65, 84};
BYTE IHDR_HDR_BYTES[] = {73, 68, 65, 84};
BYTE IHDR_BYTES_BUF[4+13+4]; //'IHDR' + data + crc

//For Reading
long long MY_PNG_READ_OFFSET = 0;

//Need to be global for custom libpng write/read fns
BYTE *ENTIRE_PNG_BUF;
long long PNG_LENGTH = 0; //ALso used for writing

struct ihdr_infos {
  uint width;
  uint height;
  BYTE bit_depth;
  BYTE colour_type;
  BYTE compression_method;
  BYTE filter_method;
  BYTE interlace_method;
};

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


typedef struct my_png_meta_s {
  png_structp read_ptr;
  png_structp write_ptr;
  png_infop info_ptr;
  png_infop end_info;
} my_png_meta;

void error(int code, char* obj, char* msg) {
  printf("%s: %s\n", obj, msg);
  exit(code);
}

void my_init_zlib(z_stream *s) {
  s->zalloc = Z_NULL;
  s->zfree = Z_NULL;
  s->opaque = Z_NULL;
  s->data_type = Z_BINARY;
  s->avail_in = 0;
}

void append_bytes(BYTE *basebuf, BYTE *inbuf, int base_offset, int inbuf_length) {
  BYTE* end_of_buf = basebuf + base_offset;
  for (int i=0;i<inbuf_length;i++) {
    end_of_buf[i] = inbuf[i];
  }
}

//We don't need to flush, since we're just memcpying
void my_png_dummy_flush(png_structp png_ptr) {
  return;
}

void my_png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length) {

   //printf("writelen: %lu\n", length);

   if (png_ptr == NULL)
      return;

   ENTIRE_PNG_BUF = realloc(ENTIRE_PNG_BUF, PNG_LENGTH + length);
   
   if (PNG_LENGTH > MAX_PNG_OUT_BYTESIZE)
      png_error(png_ptr, "Write Error: Trying to realloc > 30MB");

   append_bytes(ENTIRE_PNG_BUF, data, PNG_LENGTH, length);

   PNG_LENGTH += length;

}


//This callback depends on a global MY_PNG_READ_OFFSET
//to figure out what part of the buffer it should return.
//The default png_read uses fread(), so this is handled automatically for them
void my_png_read_fn(png_structp png_ptr, png_bytep data, png_size_t length) {

   if (png_ptr == NULL)
      return;

   //printf("PNG_LENGTH: %lld\n", PNG_LENGTH);
   //printf("MY_PNG_READ_OFFSET: %lld\n", MY_PNG_READ_OFFSET);
   //printf("Trying to read: : %lu\n", length);
   
   if (MY_PNG_READ_OFFSET > PNG_LENGTH)
      png_error(png_ptr, "Read Error: Trying to read past buffer bounds in callback");

   get_x_bytes(MY_PNG_READ_OFFSET, length, data, png_ptr->io_ptr);
   MY_PNG_READ_OFFSET += length;
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


void my_init_libpng(my_png_meta *png_meta) {

  png_structp png_read_ptr = 
    png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

  if (!png_read_ptr)
    error(-1, "libpng", "cannot init libpng read struct");

  png_infop info_ptr = png_create_info_struct(png_read_ptr);

  if (!info_ptr) {
    png_destroy_read_struct(&png_read_ptr, (png_infopp)NULL, (png_infopp)NULL);
    error(-1, "libpng", "cannot init libpng info struct");
  }

  png_infop end_info = png_create_info_struct(png_read_ptr);

  if (!end_info) {
    png_destroy_read_struct(&png_read_ptr, &info_ptr, (png_infopp)NULL);
    error(-1, "libpng", "cannot init libpng end info struct");
  }

  //write pointer
  png_structp png_write_ptr = png_create_write_struct
    (PNG_LIBPNG_VER_STRING, (png_voidp)NULL,
     NULL, NULL);

  if (!png_write_ptr)
    error(-1, "libpng", "Could not initialize write pointer");

  if (setjmp(png_jmpbuf(png_read_ptr))) {
    png_destroy_read_struct(&png_read_ptr, &info_ptr, &end_info);
    error(-1, "libpng", "setjmp error");
  }
  
  png_meta->write_ptr = png_write_ptr;
  png_meta->read_ptr = png_read_ptr;
  png_meta->info_ptr = info_ptr;
  png_meta->end_info = end_info;

}

void print_int_bytes(BYTE *buf, int x) {
  for (int i=0;i<x;i++)
    printf("%d ", buf[i]);
}

void print_chr_bytes(BYTE *buf, int x) {
  for (int i=0;i<x;i++)
    putchar(buf[i]);
}

//Takes uncompressed concated IDAT buffer
void glitch_filter(BYTE *data, ulonglong data_len, uint scanline_len, short filter) {
  for (ulonglong i=0; i<data_len; i += scanline_len) {
    printf("Glitching offset %llu\n", i);
    data[i] = filter;
  }
}

void write_idats_buffer(BYTE *raw_data, ulonglong data_len, FILE* fp) {

  printf("Taking in buffer uncompressed buffer size  %lld\n", data_len);

  //Init a new compressing zlib
  int ret;
  struct z_stream_s deflate_stream;
  my_init_zlib(&deflate_stream);
  ret = deflateInit(&deflate_stream, Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) 
    error(-1, "zlib deflate init", "ret != Z_OK");

  //This grows
  long zipped_offset = 0;
  long zipped_len = 1;
  BYTE *zipped_idats = (BYTE*)calloc(1, zipped_len);

  deflate_stream.next_in = raw_data; 
  deflate_stream.avail_in = data_len; 

      do { 

        deflate_stream.next_out = (zipped_idats + zipped_offset); 
        deflate_stream.avail_out = zipped_len - zipped_offset; 

        long prevtotal = deflate_stream.total_out;

        ret = deflate(&deflate_stream, Z_NO_FLUSH);
        
        printf("ret: %d\n", ret);
        printf("left: %u\n", deflate_stream.avail_in);

        long last_deflate_len = deflate_stream.total_out - prevtotal;
        zipped_offset += last_deflate_len;

         //needs bigger buffer
        if (ret == Z_DATA_ERROR ||
            ret == Z_BUF_ERROR  ||
            deflate_stream.avail_out <= 0) 
        {
           zipped_len = (zipped_len*2) + 1;
           printf("Setting bigger buffer (%ld)\n", zipped_len);
           zipped_idats = realloc(zipped_idats, zipped_len);
        }
      } while (deflate_stream.avail_in != 0);


    zipped_idats = realloc(zipped_idats, deflate_stream.total_out);
    printf("Decompressed to size buffer size  %ld\n", deflate_stream.total_out);
    //dump_buf_to_file("ZIPPED.buf", zipped_idats, deflate_stream.total_out);

}


int main(int argc, char* argv[]) {

  char *filename = "3row.png";

  if (argc > 1)
    filename = argv[1];

  printf("Input is '%s'\n", filename);
  FILE *fp = fopen(filename, "rb");

  if (fp == NULL) 
    error(-1, filename, "cannot open file");

  unsigned char sig[8];

  fread(sig, 1, 8, fp);
  fseek(fp, SEEK_SET, 0);

  if (png_sig_cmp(sig, 0, 8) != 0)
    error(-1, filename, "not a PNG file");

  //Take at max a 10 MB png
  long sz = 0;
  long INDX = 0;
  ENTIRE_PNG_BUF = calloc(1, 1);
  BYTE tmpbuff[IN_BUF_SIZE];
  bzero(tmpbuff, IN_BUF_SIZE);
  
  //This will read from stdin? See fastcgi spec
  while ((sz = fread(tmpbuff, 1, IN_BUF_SIZE, fp)) != 0) {
    PNG_LENGTH += sz;

    if (PNG_LENGTH >= MAX_PNG_IN_BYTESIZE)
      error(1, "input", "read too much!");
    

    ENTIRE_PNG_BUF = realloc(ENTIRE_PNG_BUF, PNG_LENGTH);
    append_bytes(ENTIRE_PNG_BUF, tmpbuff, PNG_LENGTH-sz, sz);
  }

  printf("Buf is %lld bytes\n", PNG_LENGTH);
  fclose(fp);

  my_png_meta *ps;
  ps = calloc(1, sizeof(my_png_meta));
  my_init_libpng(ps);
  
  //Normally a file, but instead make it our buffer
  ps->read_ptr->io_ptr = ENTIRE_PNG_BUF;

  void *read_io_ptr = png_get_io_ptr(ps->read_ptr);

  png_set_read_fn(ps->read_ptr, read_io_ptr, my_png_read_fn);

  //Should convert all PNG image types to RGB
  
  int transforms = 
    PNG_TRANSFORM_GRAY_TO_RGB |
    PNG_TRANSFORM_STRIP_ALPHA | 
    PNG_TRANSFORM_EXPAND;

  png_read_png(ps->read_ptr, ps->info_ptr, transforms, NULL);

  //Now that its being read and transformed, its size will differ
  PNG_LENGTH = 0; 

  void *write_io_ptr = png_get_io_ptr(ps->write_ptr);

  png_set_write_fn(ps->write_ptr, write_io_ptr, my_png_write_fn, my_png_dummy_flush);
  png_write_png(ps->write_ptr, ps->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  png_structp *tmppngread = &ps->read_ptr;
  png_structp *tmppngwrite = &ps->write_ptr;
  png_infop *tmppnginfo= &ps->info_ptr;

  png_destroy_read_struct(tmppngread, NULL, NULL);
  png_destroy_write_struct(tmppngwrite, tmppnginfo);
  free(ps);

  printf("Transformed buf is %lld bytes\n", PNG_LENGTH);

  struct z_stream_s inflate_stream;
  my_init_zlib(&inflate_stream);
  inflateInit(&inflate_stream);
  
  INDX += 8; //Skip PNG Signature
  INDX += 4; //Skip IHDR len
  
  //Get Header
  get_x_bytes(INDX, 4+13+4, IHDR_BYTES_BUF, ENTIRE_PNG_BUF);

  print_int_bytes(IHDR_BYTES_BUF, 4+13+4);
  exit(1);

  BYTE *tmpbytes = calloc(4, 1);

  long long ZIPPED_IDATS_LEN = 0; //Length of all idats as we read them
  long long UNZIPPED_IDATS_LEN = 0; //Accumulator for unzipped idats length
  BYTE *UNZIPPED_IDATS_BUF = (BYTE*)calloc(1, 1);

  //We could get this to read directly from stdin stream
  while (INDX < PNG_LENGTH) {
    
    get_x_bytes(INDX, 4, tmpbytes, ENTIRE_PNG_BUF);
    long chunk_len = _4bytesToInt(tmpbytes);
    INDX += 4; //Skip over length
    get_x_bytes(INDX, 4, tmpbytes, ENTIRE_PNG_BUF);

    if (memcmp(tmpbytes, IDAT_HDR_BYTES, 4) == 0) {

      INDX += 4; //Skip over header

      ZIPPED_IDATS_LEN += chunk_len;

      BYTE *in_chunk_bytes = (BYTE*)calloc(chunk_len, 1);
      bzero(in_chunk_bytes, chunk_len);
      get_x_bytes(INDX, chunk_len, in_chunk_bytes, ENTIRE_PNG_BUF);

      //printf("%ld\n", chunk_len);

      int ret;

      BYTE *bytes_out = (BYTE*)calloc(0, 1); //Create new output buffer

      long out_buf_len = 0;
      long bytes_uncompressed = 0; //Actual number of uncompressed bytes

      inflate_stream.next_in = in_chunk_bytes; //tell inflater its input buffer
      inflate_stream.avail_in = chunk_len; //tell inflater its input size

      do { 

        //tell inflater where to write, how much room
        inflate_stream.next_out = bytes_out; 
        inflate_stream.avail_out = out_buf_len; 

        long prevtotal = inflate_stream.total_out;
        ret = inflate(&inflate_stream, Z_FULL_FLUSH);

        /*
        printf("out len: %ld\n", out_buf_len);
        printf("%d\n", ret);
        printf("avail_in: %u\n", inflate_stream.avail_in);
        printf("avail_out: %u\n", inflate_stream.avail_out);
        printf("total_out: %lu\n", inflate_stream.total_out);
        */

        long last_inflate_len = inflate_stream.total_out - prevtotal;
        bytes_uncompressed += last_inflate_len;
        UNZIPPED_IDATS_LEN += last_inflate_len;

        //append the newly uncompressed buffer to UNZIPPED_IDATS_BUF
        UNZIPPED_IDATS_BUF = realloc(UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN);
        append_bytes(UNZIPPED_IDATS_BUF, bytes_out, UNZIPPED_IDATS_LEN-last_inflate_len, last_inflate_len);

        //usleep(5000);

        if (ret == Z_DATA_ERROR ||
            ret == Z_BUF_ERROR ||
            inflate_stream.avail_out <= 0) { //needs bigger buffer
           out_buf_len += chunk_len/10;
           //printf("Setting bigger inflate buffer (%ld)\n", out_buf_len);
           bytes_out = realloc(bytes_out, out_buf_len);
           continue;
        }

      } while (inflate_stream.avail_in != 0);


      //printf("####\n");
      //printf("Done uncompressing %ld bytes to %ld bytes\n", chunk_len, bytes_uncompressed);
      //printf("####\n");

      free(bytes_out);
      free(in_chunk_bytes);
      
      INDX += chunk_len + 4; //+ CRC
      //printf("INDX: %ld\n", INDX);

    } else {
      print_int_bytes(tmpbytes, 4); 
      printf(" -- not IDAT\n");
      INDX += chunk_len + 8;
    }
  }

  //printf("UNZIPPED_IDATS_LEN: %lldm\n", UNZIPPED_IDATS_LEN/1024/1024);
  //print_int_x_bytes(UNZIPPED_IDATS_BUF,UNZIPPED_IDATS_LEN);
  printf("Unzipped %lld bytes of data to %lld bytes\n", ZIPPED_IDATS_LEN, UNZIPPED_IDATS_LEN);

  printf("$$$$$$$$$$$$$$$$$$$$$$$$$$\n");

  //dump_buf_to_file("UNZIPPED.buf", UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN);
  //write_idats_buffer(UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN, NULL);

  free(ENTIRE_PNG_BUF);
  free(UNZIPPED_IDATS_BUF);
  inflateEnd(&inflate_stream);

  //png_init_io(png_meta.png_ptr, fp);
  //png_read_info(png_meta.png_ptr, png_meta.info_ptr);

  /*unsigned int height = png_get_image_height(png_meta.png_ptr, png_meta.info_ptr);
  unsigned int width = png_get_image_width(png_meta.png_ptr, png_meta.info_ptr);

  printf("Width: %u\n", width);
  printf("Height: %u\n", height);

  int channels = png_get_channels(png_meta.png_ptr, png_meta.info_ptr);
  int rowbytes = png_get_rowbytes(png_meta.png_ptr, png_meta.info_ptr);
  printf("Channels (bytes per pixel): %u\n", channels);
  printf("Bytes per row: %u\n", rowbytes);

  unsigned char row[rowbytes];

  for  (int j=0;j<height;j++) {
    png_read_row(png_meta.png_ptr, row, NULL);
    for (int i=0;i<rowbytes;i++)
      printf("%3u ", (unsigned int)row[i]);
    printf("\n");
  }*/

  return 0;
}
