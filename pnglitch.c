#include <stdint.h>
#include <stdlib.h>
#include <zlib.h>
#include <arpa/inet.h>
#include <fcgi_stdio.h>
#include <png.h>
#include "libs.h"

#include "pnglitch.h"
#include "debug.h"

typedef unsigned char BYTE;

unsigned char PNG_SIGNATURE[] = {137, 80, 78, 71, 13, 10, 26, 10}; //len 8
unsigned char PNG_IEND_CHUNK[] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130}; //len 12
unsigned char IDAT_HDR_BYTES[] = {73, 68, 65, 84};
unsigned char IHDR_HDR_BYTES[] = {73, 72, 68, 82};

unsigned char *zip_idats(unsigned char *raw_data, ulong data_len, long long *compressed_length) {

  DEBUG_PRINT(("Zipping glitched buffer length %ld\n", data_len));

  //Init a new compressing zlib
  int ret;
  struct z_stream_s deflate_stream;
  my_init_zlib(&deflate_stream);
  ret = deflateInit(&deflate_stream, Z_NO_COMPRESSION);//Z_DEFAULT_COMPRESSION);
  if (ret != Z_OK) 
    error_fatal(-1, "zlib deflate init", "ret != Z_OK");

  long zipped_offset = 0;
  long zipped_len = 1000;
  //This grows
  unsigned char *zipped_idats = calloc(zipped_len, 1);

  deflate_stream.next_in = raw_data; 
  deflate_stream.avail_in = data_len; 
  do {  // This compresses

    deflate_stream.next_out = (zipped_idats + zipped_offset); 
    deflate_stream.avail_out = zipped_len - zipped_offset; 

    long prevtotal = deflate_stream.total_out;

    ret = deflate(&deflate_stream, Z_FINISH);

    long last_deflate_len = deflate_stream.total_out - prevtotal;
    zipped_offset += last_deflate_len;

    //needs bigger buffer
    if (ret == Z_STREAM_END) 
      break;
    else if ( ret == Z_DATA_ERROR ||
              ret == Z_BUF_ERROR  ||
             (ret == Z_OK && deflate_stream.avail_out == 0)) 
    {
      zipped_len = (zipped_len*2) + 1;
      DEBUG_PRINT(("Setting bigger buffer (%ld)\n", zipped_len));
      zipped_idats = realloc(zipped_idats, zipped_len);
      continue;
    }
    else if (ret == Z_DATA_ERROR || ret == Z_DATA_ERROR) {
      DEBUG_PRINT(("Error in zip_idats: ret was %d, \
            total out was %ld, last deflate len was %ld\n",
            ret, deflate_stream.total_out, last_deflate_len));
      free(zipped_idats);
      return NULL;
    }
  } while (1);

  zipped_idats = realloc(zipped_idats, deflate_stream.total_out);
  DEBUG_PRINT(("Compressed %ld size buffer to %ld\n", data_len, deflate_stream.total_out));

  *compressed_length = deflate_stream.total_out;

  deflateEnd(&deflate_stream);

  return(zipped_idats);
}


//Takes uncompressed concated IDAT buffer
void glitch_random_filter(unsigned char *data, unsigned long data_len, unsigned int scanline_len) {
  DEBUG_PRINT(("\nGlitching offsets with random\n"));
  for (unsigned long i=0; i<data_len; i += scanline_len) {
    DEBUG_PRINT(("%lu (%d)", i, data[i]));
    data[i] = rand()%5;
  }
}

void glitch_filter(unsigned char *data, unsigned long data_len, unsigned int scanline_len, int filter) {
  DEBUG_PRINT(("\nGlitching offsets with %d\n", filter));
  for (unsigned long i=0; i<data_len; i += scanline_len) {
    DEBUG_PRINT(("%lu (%d) ", i, data[i]));
    data[i] = filter;
  }
}


void glitch_random(unsigned char *data, unsigned long data_len, unsigned int scanline_len, float freq) {

  srand(84677210);
  long glitches = (long) (((double) data_len) * freq);

  // The plus one includes the filter byte
  for (uint32_t i = 0; i < glitches; i++) {

    uint64_t spot = ((rand() << 31) | rand())%data_len;

    // Protects filter byte from being overwritten
    if ((spot % scanline_len) == 0)
      continue;

    data[spot] = rand()%256;
  }
}

void write_glitched_image(unsigned char *glitched_idats, 
    long glitched_idats_len, unsigned char *ihdr_bytes_buf, FILE* fp) {


  uint32_t ihdr_size = htonl(13);
  uint32_t idat_len = htonl(glitched_idats_len);
  uint32_t idat_data_crc = crc32(0L, glitched_idats, glitched_idats_len); 
  uint32_t idat_ihdr_crc = crc32(0L, IDAT_HDR_BYTES, 4); 
  uint32_t idat_crc = htonl(crc32_combine(idat_ihdr_crc, idat_data_crc, glitched_idats_len));

  fwrite(PNG_SIGNATURE, 1, 8, fp);
  fwrite(&ihdr_size, sizeof(ihdr_size), 1, fp);
  fwrite(ihdr_bytes_buf, 1, 4+13+4, fp);
  fwrite(&idat_len, sizeof(idat_len), 1, fp);
  fwrite(IDAT_HDR_BYTES, 1, 4, fp);
  fwrite(glitched_idats, 1, glitched_idats_len, fp); //
  fwrite(&idat_crc, sizeof(idat_crc), 1, fp);
  fwrite(PNG_IEND_CHUNK, 1, 12, fp);

}
