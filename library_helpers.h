#ifndef LIBRARY_HELPERS_H_   
#define LIBRARY_HELPERS_H_

typedef struct my_png_meta_s {
  png_structp read_ptr;
  png_structp write_ptr;
  png_infop info_ptr;
  png_infop end_info;
} my_png_meta;

//Most of these are callbacks or init functions
void my_png_dummy_flush(png_structp png_ptr);
void my_png_write_fn(png_structp png_ptr, png_bytep data, png_size_t length);
void my_png_read_fn(png_structp png_ptr, png_bytep data, png_size_t length);
void my_init_libpng(my_png_meta *png_meta);
void my_init_zlib(z_stream *s);
#endif 
