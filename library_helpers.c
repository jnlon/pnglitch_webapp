#include <zlib.h>
#include <png.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "library_helpers.h"
#include "debug.h"

void my_init_zlib(z_stream *s) {
  s->zalloc = Z_NULL;
  s->zfree = Z_NULL;
  s->opaque = Z_NULL;
  s->data_type = Z_BINARY;
  s->avail_in = 0;
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

   get_x_bytes(MY_PNG_READ_OFFSET, length, data, ENTIRE_PNG_BUF);
   MY_PNG_READ_OFFSET += length;
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
