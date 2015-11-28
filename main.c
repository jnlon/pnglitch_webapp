#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <zlib.h>
#include <unistd.h>
#include <stdint.h>
#include <arpa/inet.h>

#include "library_helpers.h"
#include "common.h"
#include "debug.h"


//Globals, needed for callbacks
long long MY_PNG_READ_OFFSET = 0;
BYTE *ENTIRE_PNG_BUF;
long long PNG_LENGTH = 0; 

/*
 * Use Libpng to tansform the input into into RGB format 
 * (basically re-encode image using all filter methods)
 * Using the write-callback method, store this data in a shared buffer.
 * Then, glitch this buffer manually
 *
 * TODO: 
 * -Set no compression when writing image to buffer
 *
 */

BYTE PNG_SIGNATURE[] = {137, 80, 78, 71, 13, 10, 26, 10}; //8
BYTE PNG_IEND_CHUNK[] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130}; //12
BYTE IDAT_HDR_BYTES[] = {73, 68, 65, 84};
BYTE IHDR_HDR_BYTES[] = {73, 72, 68, 82};
BYTE IHDR_BYTES_BUF[4+13+4]; //'IHDR' + data + crc

struct ihdr_infos_s {

  ulong width;
  ulong height;
  ulong bit_depth;
  ulong color_type;
  ulong compression_type;
  ulong filter_method;
  ulong interlace_type;

  ulong bytes_per_pixel;
  ulong scanline_len;

};

//Takes uncompressed concated IDAT buffer
void glitch_filter(BYTE *data, ulonglong data_len, uint scanline_len, int filter) {
  for (ulonglong i=0; i<data_len; i += scanline_len) {
    printf("Glitching offset %llu -> %d\n", i, data[i]);
    data[i] = filter;
  }
}

BYTE *zip_idats(BYTE *raw_data, ulong data_len, uint32_t *compressed_length) {

  //printf("Taking in buffer uncompressed buffer size  %ld\n", data_len);

  //Init a new compressing zlib
  int ret;
  struct z_stream_s deflate_stream;
  my_init_zlib(&deflate_stream);
  ret = deflateInit(&deflate_stream, Z_NO_COMPRESSION);
  if (ret != Z_OK) 
    error(-1, "zlib deflate init", "ret != Z_OK");

  //This grows
  long zipped_offset = 0;
  long zipped_len = 1000;
  printf("GOING IN: %lu\n", data_len);
  BYTE *zipped_idats = (BYTE*)calloc(zipped_len, 1);

  deflate_stream.next_in = raw_data; 
  deflate_stream.avail_in = data_len; 
      do {  // This compresses
        //deflate_stream.next_in = raw_data; 

        deflate_stream.next_out = (zipped_idats + zipped_offset); 
        deflate_stream.avail_out = zipped_len - zipped_offset; 

        long prevtotal = deflate_stream.total_out;

        //printf("avail_out1: %u\n", deflate_stream.avail_out);
        ret = deflate(&deflate_stream, Z_FINISH);
        //printf("avail_out2: %u\n", deflate_stream.avail_out);
        
        // Shrink buffer size as it's being read
        //raw_data = realloc(raw_data, deflate_stream.avail_in);
        
        long last_deflate_len = deflate_stream.total_out - prevtotal;
        zipped_offset += last_deflate_len;

        if (ret == Z_STREAM_END)
          break;

         //needs bigger buffer
        if (ret == Z_DATA_ERROR ||
            ret == Z_BUF_ERROR  ||
            ((ret == Z_OK) && deflate_stream.avail_out == 0)) 
        {
           zipped_len = (zipped_len*2) + 1;
           printf("Setting bigger buffer (%ld)\n", zipped_len);
           zipped_idats = realloc(zipped_idats, zipped_len);
           continue;
        }
      } while (1);

    zipped_idats = realloc(zipped_idats, deflate_stream.total_out);
    printf("Compressed %ld size buffer to %ld\n", data_len, deflate_stream.total_out);

    *compressed_length = deflate_stream.total_out;

    deflateEnd(&deflate_stream);

    //free(raw_data); raw_data = NULL;

    return(zipped_idats);
}


int main(int argc, char* argv[]) {

  if (argc <= 1)
    error(1,"input", "need filename");

  char *filename = argv[1];

  printf("Input is '%s'\n", filename);
  FILE *fp = fopen(filename, "rb");

  if (fp == NULL) 
    error(-1, filename, "cannot open file");

  unsigned char sig[8];

  fread(sig, 1, 8, fp);
  fseek(fp, SEEK_SET, 0);

  if (png_sig_cmp(sig, 0, 8) != 0)
    error(-1, filename, "not a PNG file");

  long sz = 0;
  long INDX = 0;
  ENTIRE_PNG_BUF = calloc(1, 1);
  BYTE tmpbuff[IN_BUF_SIZE];
  bzero(tmpbuff, IN_BUF_SIZE);
  
  //This will read max 10MB from stdin? See fastcgi spec
  while ((sz = fread(tmpbuff, 1, IN_BUF_SIZE, fp)) != 0) {
    PNG_LENGTH += sz;

    if (PNG_LENGTH >= MAX_PNG_IN_BYTESIZE)
      error(1, "input", "read too much!");
    
    ENTIRE_PNG_BUF = realloc(ENTIRE_PNG_BUF, PNG_LENGTH);
    append_bytes(ENTIRE_PNG_BUF, tmpbuff, PNG_LENGTH-sz, sz);
  }

  printf("Buf is %lld bytes\n", PNG_LENGTH);
  fclose(fp);

  my_png_meta *pm = calloc(1, sizeof(my_png_meta));
  my_init_libpng(pm);
  
  //Normally a file, but instead make it our buffer
  void *read_io_ptr = png_get_io_ptr(pm->read_ptr);
  png_set_read_fn(pm->read_ptr, read_io_ptr, my_png_read_fn);

  //Should convert all PNG image types to RGB
  int transforms = 
    PNG_TRANSFORM_GRAY_TO_RGB |
    PNG_TRANSFORM_STRIP_ALPHA | 
    PNG_TRANSFORM_EXPAND;

  png_read_png(pm->read_ptr, pm->info_ptr, transforms, NULL);
  
  //Now that its being read and transformed, its size will differ
  PNG_LENGTH = 0; 

  //Lets collect our metadata
  struct ihdr_infos_s ihdr_infos;
  ihdr_infos.bit_depth        = png_get_bit_depth(pm->read_ptr, pm->info_ptr);
  ihdr_infos.color_type       = png_get_color_type(pm->read_ptr, pm->info_ptr);
  ihdr_infos.filter_method    = png_get_filter_type(pm->read_ptr, pm->info_ptr);
  ihdr_infos.compression_type = png_get_compression_type(pm->read_ptr, pm->info_ptr);
  ihdr_infos.interlace_type   = png_get_interlace_type(pm->read_ptr, pm->info_ptr);
  ihdr_infos.height           = png_get_image_height(pm->read_ptr, pm->info_ptr);
  ihdr_infos.width            = png_get_image_width(pm->read_ptr, pm->info_ptr);

  if (ihdr_infos.color_type != 2)
    error(1, "ihdr_infos", "Image was not correctly converted to RGB");

  //Just in case we want to enable alpha, etc
  switch(ihdr_infos.color_type) {
    case 0:  //greyscale
    case 3:  //indexed
      ihdr_infos.bytes_per_pixel = 1;
    break;
    case 4: ihdr_infos.bytes_per_pixel = 2; break; //greyscale w/ alpha 
    case 2: ihdr_infos.bytes_per_pixel = 3; break; //Truecolour (RGB)
    case 6: ihdr_infos.bytes_per_pixel = 4; break; //Truecolour w/ alpha
    default: error(1, "ihdr_infos", "Unknown image type"); 
  }

  ihdr_infos.scanline_len = (ihdr_infos.bytes_per_pixel * ihdr_infos.width) + 1;

  printf("HEIGHT: %lu\n", ihdr_infos.height);
  printf("WIDTH: %lu\n", ihdr_infos.width);
  printf("BIT_DEPTH: %lu\n", ihdr_infos.bit_depth);

  // Don't compress, since we are merely copying it to memory,
  // we  will be decompressing it again anyway
  png_set_compression_level(pm->write_ptr, Z_NO_COMPRESSION);

  void *write_io_ptr = png_get_io_ptr(pm->write_ptr);
  png_set_write_fn(pm->write_ptr, write_io_ptr, my_png_write_fn, my_png_dummy_flush);

  //Using callback my_png_write_fn, output is written to ENTIRE_PNG_BUF
  //PNG_LENGTH will be updated too
  png_write_png(pm->write_ptr, pm->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  png_destroy_read_struct(&pm->read_ptr, NULL, NULL);
  png_destroy_write_struct(&pm->write_ptr, &pm->info_ptr);
  free(pm);

  printf("Transformed buf is %lld bytes\n", PNG_LENGTH);

  //Now that libpng is done converting the image, 
  //and we have it in the buffer, lets process by hand with zlib
  
  //Should we preserve bKGD, pHYS, tIME?

  printf("#### Stage 1: Transform PNG complete ####\n");

  struct z_stream_s inflate_stream;
  my_init_zlib(&inflate_stream);
  inflateInit(&inflate_stream);
  
  INDX += 8; //Skip PNG Signature
  INDX += 4; //Skip IHDR len
  
  //Get Header
  get_x_bytes(INDX, 4+13+4, IHDR_BYTES_BUF, ENTIRE_PNG_BUF);

  INDX += 4+13+4; //Skip All of IHDR

  BYTE *tmpbytes = calloc(4, 1);

  long long ZIPPED_IDATS_LEN = 0; //Length of all idats as we read them
  BYTE *UNZIPPED_IDATS_BUF = calloc(1, 1);
  int chunk_count = 0;

  long out_buf_len = 1;
  long out_buf_offset = 0;

  //We could get this to read directly from stdin stream
  while (INDX < PNG_LENGTH) {
    
    //Get the chunk length
    get_x_bytes(INDX, 4, tmpbytes, ENTIRE_PNG_BUF);
    INDX += 4; 
    long chunk_len = _4bytesToInt(tmpbytes);

    //Now, what is the header name?
    get_x_bytes(INDX, 4, tmpbytes, ENTIRE_PNG_BUF);

    chunk_count += 1;

    if (memcmp(tmpbytes, IDAT_HDR_BYTES, 4) == 0) {

      INDX += 4; //Skip over header

      ZIPPED_IDATS_LEN += chunk_len;

      BYTE *in_chunk_bytes = (BYTE*)calloc(chunk_len, 1);
      bzero(in_chunk_bytes, chunk_len);
      get_x_bytes(INDX, chunk_len, in_chunk_bytes, ENTIRE_PNG_BUF);

      UNZIPPED_IDATS_BUF = realloc(UNZIPPED_IDATS_BUF, out_buf_len);

      inflate_stream.next_in = in_chunk_bytes; //tell inflater its input buffer
      inflate_stream.avail_in = chunk_len; //tell inflater its input size

      do {  //Uncompress this idat chunk

        //tell inflater where to write, how much room
        inflate_stream.next_out = UNZIPPED_IDATS_BUF + out_buf_offset; 
        inflate_stream.avail_out = out_buf_len - out_buf_offset; 

        long prevtotal = inflate_stream.total_out;
        int ret = inflate(&inflate_stream, Z_NO_FLUSH);
        //printf("%d\n",ret);

        long last_inflate_len = inflate_stream.total_out - prevtotal;
        out_buf_offset += last_inflate_len;

        if (ret == Z_DATA_ERROR ||
            ret == Z_BUF_ERROR ||
            inflate_stream.avail_out <= 0) //needs bigger buffer
        { 
           out_buf_len = (out_buf_len*2) + 1;
           UNZIPPED_IDATS_BUF = realloc(UNZIPPED_IDATS_BUF, out_buf_len);
           continue;
        }

      } while (inflate_stream.avail_in != 0);

      free(in_chunk_bytes);
      
      INDX += chunk_len + 4; //+ CRC
      //printf("INDX: %ld\n", INDX);

    } else {
      printf("Chunk %d Not IDAT: ", chunk_count);
      print_chr_bytes(tmpbytes, 4); 
      putchar('\n');
      INDX += chunk_len + 8;
    }
  }
  
  long long UNZIPPED_IDATS_LEN = inflate_stream.total_out; 
  UNZIPPED_IDATS_BUF = realloc(UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN);
  free(ENTIRE_PNG_BUF);
  inflateEnd(&inflate_stream);

  printf("#### Stage 2: UNZIP transformed PNG complete ####\n");

  printf("Unzipped %lld bytes of data to %lld bytes\n", ZIPPED_IDATS_LEN, UNZIPPED_IDATS_LEN);


  printf("#### Stage 3: Glitch complete ####\n");

  for (int ftype=0;ftype<5;ftype++) {

    glitch_filter(UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN, ihdr_infos.scanline_len, ftype);

    uint32_t REZIPPED_IDATS_LEN = 0;
    BYTE *REZIPPED_IDATS = zip_idats(UNZIPPED_IDATS_BUF, UNZIPPED_IDATS_LEN, &REZIPPED_IDATS_LEN);

    printf("#### Stage 4: Compress idats complete ####\n");

    //Now write thing to file:
    // -Sig
    // -IHDR
    // -[optional: others?]
    // -Idats
    // -IEND

    uint32_t ihdr_size = htonl(13);
    uint32_t idat_len = htonl(REZIPPED_IDATS_LEN);
    uint32_t idat_data_crc = crc32(0L, REZIPPED_IDATS, REZIPPED_IDATS_LEN); 
    uint32_t idat_ihdr_crc = crc32(0L, IDAT_HDR_BYTES, 4); 
    uint32_t idat_crc = htonl(crc32_combine(idat_ihdr_crc, idat_data_crc, REZIPPED_IDATS_LEN));

    char filename[20];
    snprintf(filename, 20, "OUT-%d.png", ftype);

    FILE *f = fopen(filename, "wb");

    fwrite(PNG_SIGNATURE, 1, 8, f);
    fwrite(&ihdr_size, sizeof(ihdr_size), 1, f);
    fwrite(IHDR_BYTES_BUF, 1, 4+13+4, f);
    fwrite(&idat_len, sizeof(idat_len), 1, f);
    fwrite(IDAT_HDR_BYTES, 1, 4, f);
    fwrite(REZIPPED_IDATS, 1, REZIPPED_IDATS_LEN, f);
    fwrite(&idat_crc, sizeof(idat_crc), 1, f);
    fwrite(PNG_IEND_CHUNK, 1, 12, f);

    fclose(f);

    free(REZIPPED_IDATS);
  }

  return 0;
}
