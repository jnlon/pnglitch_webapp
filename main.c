#define _GNU_SOURCE
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <fcgi_stdio.h>
#include <zlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>

#include "library_helpers.h"
#include "fcgi_helpers.h"
#include "common.h"
#include "debug.h"
#include "web.h"

//Globals, needed for callbacks
long long MY_PNG_READ_OFFSET = 0;
BYTE *ENTIRE_PNG_BUF;
long long PNG_LENGTH = 0; 

/*
 * Use Libpng to tansform the input into into RGB format 
 * (basically re-encode image using all filter methods)
 * Using the write-callback method, store this data in a shared buffer.
 * Then, glitch this buffer manually
 */

BYTE PNG_SIGNATURE[] = {137, 80, 78, 71, 13, 10, 26, 10}; //len 8
BYTE PNG_IEND_CHUNK[] = {0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130}; //len 12
BYTE IDAT_HDR_BYTES[] = {73, 68, 65, 84};
BYTE IHDR_HDR_BYTES[] = {73, 72, 68, 82};
BYTE IHDR_BYTES_BUF[4+13+4]; //'IHDR' + data + crc

struct ihdr_infos_s {
  //libpng provided
  ulong width;
  ulong height;
  ulong bit_depth;
  ulong color_type;
  ulong compression_type;
  ulong filter_method;
  ulong interlace_type;

  //Calculated
  ulong bytes_per_pixel;
  ulong scanline_len;
};

//Takes uncompressed concated IDAT buffer
void glitch_random_filter(BYTE *data, ulonglong data_len, uint scanline_len) {
  for (ulonglong i=0; i<data_len; i += scanline_len) {
    DEBUG_PRINT(("Glitching offset %llu -> %d\n", i, data[i]));
    data[i] = rand()%5;
  }
}

void glitch_filter(BYTE *data, ulonglong data_len, uint scanline_len, int filter) {
  for (ulonglong i=0; i<data_len; i += scanline_len) {
    DEBUG_PRINT(("Glitching offset %llu -> %d\n", i, data[i]));
    data[i] = filter;
  }
}


void glitch_random(BYTE *data, ulonglong data_len, uint scanline_len, double freq) {

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

BYTE *zip_idats(BYTE *raw_data, ulong data_len, long long *compressed_length) {

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
  BYTE *zipped_idats = calloc(zipped_len, 1);

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
      print_error_html("Error compressing PNG image!");
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


void begin(char* infname_sans_ext, BYTE *png_buf, ulong png_length) {

  MY_PNG_READ_OFFSET = 0;
  PNG_LENGTH = png_length; 
  ENTIRE_PNG_BUF = png_buf;

  if (png_sig_cmp(ENTIRE_PNG_BUF, 0, 8) != 0) {
    print_error_html("Upload is not a valid PNG file!");
    return;
  }


  DEBUG_PRINT(("Buf is %lld bytes\n", PNG_LENGTH));

  my_png_meta *pm = calloc(1, sizeof(my_png_meta));
  my_init_libpng(pm);

  //If libpng errors, we end up here
  if (setjmp(png_jmpbuf(pm->read_ptr))) {
    DEBUG_PRINT(("libpng called setjmp!"));
    png_destroy_read_struct(&pm->read_ptr, &pm->info_ptr, &pm->end_info);
    print_error_html("Error processing image! It's probably corrupt!");
    free(pm);
    return;
  }

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

  if (ihdr_infos.color_type != 2) {
    print_error_html("Image was not correctly converted to RGB");
    DEBUG_PRINT(("Looks like libpng could not correctly convert file to RGB"));
    return;
  }

  //Just in case we want to enable alpha, etc
  switch(ihdr_infos.color_type) {
    case 0:  //greyscale
    case 3:  //indexed
      ihdr_infos.bytes_per_pixel = 1;
      break;
    case 4: ihdr_infos.bytes_per_pixel = 2; break; //greyscale w/ alpha 
    case 2: ihdr_infos.bytes_per_pixel = 3; break; //Truecolour (RGB)
    case 6: ihdr_infos.bytes_per_pixel = 4; break; //Truecolour w/ alpha
    default: error_fatal(1, "ihdr_infos", "Unknown image type"); 
  }

  ihdr_infos.scanline_len = (ihdr_infos.bytes_per_pixel * ihdr_infos.width) + 1;

  DEBUG_PRINT(("HEIGHT: %lu\n", ihdr_infos.height));
  DEBUG_PRINT(("WIDTH: %lu\n", ihdr_infos.width));
  DEBUG_PRINT(("BIT_DEPTH: %lu\n", ihdr_infos.bit_depth));

  // Don't compress, since we are merely copying it to memory,
  // we  will be decompressing it again anyway
  png_set_compression_level(pm->write_ptr, Z_NO_COMPRESSION);

  void *write_io_ptr = png_get_io_ptr(pm->write_ptr);
  png_set_write_fn(pm->write_ptr, write_io_ptr, my_png_write_fn, my_png_dummy_flush);

  png_set_filter(pm->write_ptr, 0,
      PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
      PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
      PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
      PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG  |
      PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH);

  //Using callback my_png_write_fn, output is written to ENTIRE_PNG_BUF
  //PNG_LENGTH will be updated too
  png_write_png(pm->write_ptr, pm->info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

  my_deinit_libpng(pm);

  DEBUG_PRINT(("libpng output buf is %lld bytes\n", PNG_LENGTH));

  //Now that libpng is done converting the image, 
  //and we have it in the buffer, we process it by hand with zlib

  struct z_stream_s inflate_stream;
  my_init_zlib(&inflate_stream);
  inflateInit(&inflate_stream);

  long pngi = 0;

  pngi += 8; //Skip PNG Signature
  pngi += 4; //Skip IHDR len

  //Get Header
  get_x_bytes(pngi, 4+13+4, IHDR_BYTES_BUF, ENTIRE_PNG_BUF);

  pngi += 4+13+4; //Skip All of IHDR

  BYTE tmpbytes[4];

  //When we run into non-idat chunks, we will want to preserve them.
  //The spec says there'e no chunk that NEEDS to go after IDAT,
  //so we will simply concatenate all of these chunks into a buffer
  //then write them all at once after the IHDR
  
  //TODO: instead of using PNGI, use pointer with arithmetic

  long long zipped_idats_len = 0; //Length of all idats as we read them
  BYTE *unzipped_idats_buf = calloc(1, 1);

  int chunk_count = 0;

  long unzip_buf_len = 1;
  long unzip_buf_offset = 0;

  while (pngi < PNG_LENGTH) {

    //Get the chunk length
    get_x_bytes(pngi, 4, tmpbytes, ENTIRE_PNG_BUF);
    pngi += 4; 
    long chunk_len = _4bytesToInt(tmpbytes);

    //Now, what is the header name?
    get_x_bytes(pngi, 4, tmpbytes, ENTIRE_PNG_BUF);

    chunk_count += 1;

    if (memcmp(tmpbytes, IDAT_HDR_BYTES, 4) == 0) {

      DEBUG_PRINT("Chunk %d is idat\n");

      pngi += 4; //Skip over header

      zipped_idats_len += chunk_len;

      BYTE *in_chunk_bytes = calloc(chunk_len, 1);
      bzero(in_chunk_bytes, chunk_len);
      get_x_bytes(pngi, chunk_len, in_chunk_bytes, ENTIRE_PNG_BUF);

      unzipped_idats_buf = realloc(unzipped_idats_buf, unzip_buf_len);

      inflate_stream.next_in = in_chunk_bytes; //tell inflater its input buffer
      inflate_stream.avail_in = chunk_len; //tell inflater its input size

      //uncompress this idat
      do { 

        //tell inflater where to write, how much room
        inflate_stream.next_out = unzipped_idats_buf + unzip_buf_offset; 
        inflate_stream.avail_out = unzip_buf_len - unzip_buf_offset; 

        long prevtotal = inflate_stream.total_out;
        int ret = inflate(&inflate_stream, Z_NO_FLUSH);

        long last_inflate_len = inflate_stream.total_out - prevtotal;
        unzip_buf_offset += last_inflate_len;

        if (ret == Z_DATA_ERROR ||
            ret == Z_BUF_ERROR ||
            inflate_stream.avail_out <= 0) //needs bigger buffer
        { 
          unzip_buf_len = (unzip_buf_len*2) + 1;
          unzipped_idats_buf = realloc(unzipped_idats_buf, unzip_buf_len);
          continue;
        }
        else if (ret == Z_DATA_ERROR || ret == Z_DATA_ERROR) {

          DEBUG_PRINT(( "zlib error when decompresing!\n \
                        Did libpng return a bad image?\n \
                        last return was '%d'\n \
                        total output was '%ld'\n \
                        last inflate len was '%ld' \n",
                ret, inflate_stream.total_out, last_inflate_len));

          print_error_html("Error processing image!");
          free(in_chunk_bytes);
          free(unzipped_idats_buf);
          free(ENTIRE_PNG_BUF);
          return;
        }
      } while (inflate_stream.avail_in != 0);

      free(in_chunk_bytes);
      pngi += chunk_len + 4; //+ CRC

    } else {
      DEBUG_PRINT(("Chunk %d not IDAT:\n", chunk_count));

      pngi += chunk_len + 8;
    }
  }

  long long unzipped_idats_len = inflate_stream.total_out; 
  unzipped_idats_buf = realloc(unzipped_idats_buf, unzipped_idats_len);
  free(ENTIRE_PNG_BUF);
  inflateEnd(&inflate_stream);

  DEBUG_PRINT(("Unzipped %lld bytes of data to %lld bytes\n", zipped_idats_len, unzipped_idats_len));

  char output_dir[] = "pnglitch_c_output";

  int mkdir_ret = mkdir("pnglitch_c_output", S_IRWXU);

  if (mkdir_ret == -1 && errno != EEXIST)
    error_fatal(1, "problem creating directory", strerror(errno));
  else if (access("pnglitch_c_output", W_OK | X_OK))
    error_fatal(1, "Problem accessing directory", strerror(errno));

  const int path_max_len = 100;
  char *out_file_paths = malloc(path_max_len*7);

  for (int g=0;g<7;g++) {

    switch(g) {
      case 5:
        glitch_random(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len, .0005);
        break;
      case 6:
        glitch_random_filter(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len);
        break;
      default:
        glitch_filter(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len, g);
    }

    long long rezipped_idats_len = 0;
    BYTE *REZIPPED_IDATS = zip_idats(unzipped_idats_buf, unzipped_idats_len, &rezipped_idats_len);

    if (REZIPPED_IDATS == NULL) {
      free (unzipped_idats_buf);
      continue;
    }

    //Now write thing to file:
    // -Sig
    // -IHDR
    // -[optional: others?]
    // -Idats
    // -IEND

    uint32_t ihdr_size = htonl(13);
    uint32_t idat_len = htonl(rezipped_idats_len);
    uint32_t idat_data_crc = crc32(0L, REZIPPED_IDATS, rezipped_idats_len); 
    uint32_t idat_ihdr_crc = crc32(0L, IDAT_HDR_BYTES, 4); 
    uint32_t idat_crc = htonl(crc32_combine(idat_ihdr_crc, idat_data_crc, rezipped_idats_len));

    char* path = out_file_paths + (g * path_max_len);

    snprintf(path, path_max_len, "%s/%s-%d.png", output_dir, infname_sans_ext, g);

    DEBUG_PRINT(("Output file name is %s", path));

    FILE *outfp = fopen(path, "wb");

    fwrite(PNG_SIGNATURE, 1, 8, outfp);
    fwrite(&ihdr_size, sizeof(ihdr_size), 1, outfp);
    fwrite(IHDR_BYTES_BUF, 1, 4+13+4, outfp);
    fwrite(&idat_len, sizeof(idat_len), 1, outfp);
    fwrite(IDAT_HDR_BYTES, 1, 4, outfp);
    fwrite(REZIPPED_IDATS, 1, rezipped_idats_len, outfp);
    fwrite(&idat_crc, sizeof(idat_crc), 1, outfp);
    fwrite(PNG_IEND_CHUNK, 1, 12, outfp);

    fclose(outfp);
    free(REZIPPED_IDATS);
  }

  char *ofp = out_file_paths;
  int o = path_max_len; //offset
  print_success_html(ofp + (o*0), ofp + (o*1), ofp + (o*2),
      ofp + (o*3), ofp + (o*4),ofp + (o*5), ofp + (o*6));

  free(out_file_paths);
  free(unzipped_idats_buf);
}

int main(int argc, char* argv[]) {

  success_template = load_html_template(SUCCESS_FILE_PATH);
  error_template = load_html_template(ERROR_FILE_PATH);

  while (FCGI_Accept() >= 0) {

    printf("content-type: text/html\r\n\r\n");

    long content_length = get_content_length();

    if (content_length <= 0)
      continue;

    char form_boundary[MAX_FORM_BOUNDARY_LENGTH];
    memset(form_boundary, MAX_FORM_BOUNDARY_LENGTH, 1);

    int form_boundary_len = get_form_boundary(form_boundary);

    if (form_boundary_len <= 0)
      continue;

    form_boundary[form_boundary_len] = '\0';

    char* form_meta_buf = calloc(MAX_FORM_META_LENGTH, 1);
    int form_meta_buf_sz  = get_form_meta_buf(form_meta_buf);

    if (form_meta_buf_sz <= 0)
      continue;

    form_meta_buf = realloc(form_meta_buf, form_meta_buf_sz);

    char form_filename[MAX_FILENAME_LENGTH];
    memset(form_filename, MAX_FILENAME_LENGTH, 1);

    int filename_sz = get_form_filename(form_meta_buf, form_filename);
    form_filename[filename_sz] = '\0';

    //printf("Filename of uploaded file: %s\n", form_filename);

    if (filename_sz <= 0)
      continue;

    BYTE *png_buf = calloc(content_length, 1);

    long png_length = get_uploaded_file_buf(png_buf, content_length,
        form_boundary, form_boundary_len);

    DEBUG_PRINT(("Size of uploaded png: %ld\n", png_length));

    if (png_length <= 0)
      continue;

    png_buf = realloc(png_buf, png_length);

    begin(form_filename, png_buf, png_length);

    free(form_meta_buf);
  }

  free(success_template);
  free(error_template);

  return 0;
}
