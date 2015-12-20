#define _GNU_SOURCE
#include <png.h>
#include <string.h>
#include <stdlib.h>
#include <fcgi_stdio.h>
#include <zlib.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>
#include <libgen.h>

#define DEBUG

#include "libs.h"
#include "webio.h"
#include "globals.h"
#include "bufs.h"
#include "pnglitch.h"
#include "debug.h"

//To properly free memory from fcgi library
void OS_LibShutdown(void);

//Globals, needed for callbacks
long long MY_PNG_READ_OFFSET;
unsigned char *ENTIRE_PNG_BUF;
long long PNG_LENGTH; 

/* Use Libpng to tansform the input into into RGB format 
 * (basically re-encode image using all filter methods)
 * Using the write-callback method, store this data in a shared buffer.
 * Then, glitch this buffer manually
 */

void begin(char* infname_sans_ext, unsigned char *png_buf, long long png_length) {

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
    DEBUG_PRINT(("libpng called setjmp!\n"));
    my_deinit_libpng(pm);
    free(ENTIRE_PNG_BUF);
    print_error_html("Error processing (likely corrupt) image");
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

  //Now that it was read and transformed, its size will differ
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
    print_error_html("Error processing image!");
    DEBUG_PRINT(("Looks like libpng could not correctly convert to RGB\n"));
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

  //Make sure we use all filters
  png_set_filter(pm->write_ptr, 0,
      PNG_FILTER_NONE  | PNG_FILTER_VALUE_NONE |
      PNG_FILTER_SUB   | PNG_FILTER_VALUE_SUB  |
      PNG_FILTER_UP    | PNG_FILTER_VALUE_UP   |
      PNG_FILTER_AVG   | PNG_FILTER_VALUE_AVG  |
      PNG_FILTER_PAETH | PNG_FILTER_VALUE_PAETH);

  //Set our comment
  struct png_text_struct comment_struct;

  comment_struct.compression = -1;
  comment_struct.key = "Glitched by pnglitch.me";
  comment_struct.text = NULL;
  comment_struct.text_length = 0;
  
  png_set_text(pm->write_ptr, pm->info_ptr, &comment_struct, 1);

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

  //Pointer to keep track of where we are
  unsigned char *pngp = ENTIRE_PNG_BUF;

  //Skip PNG Signature
  pngp += 8; 

  //Get Header
  unsigned char ihdr_bytes_buf[4+4+13+4]; // size + label + content + crc
  buf_read(ihdr_bytes_buf, &pngp, 4+4+13+4);

  //When we run into non-idat chunks, we will want to preserve them.
  //The spec says there'e no chunk that NEEDS to go after IDAT,
  //so we will simply concatenate all of these chunks into a buffer
  //then write them all at once after the IHDR
  
  //ancillary chunks, eg comments
  unsigned char *ancil_chunks_buf = calloc(1,1);
  long long ancil_chunks_len = 0;

  unsigned char *unzipped_idats_buf = calloc(1, 1);
  long unzip_buf_len = 1;
  long unzip_buf_offset = 0;

  long long zipped_idats_len = 0; //Length of all idats as we read them

  unsigned long accum_png_len = 8 + (4+4+13+4);

  int chunk_count = 0;

  while (1) {
    unsigned char chunk_label[4];
    unsigned char chunk_len_buf[4];

    DEBUG_PRINT(("Reading chunk len\n"));
    buf_read(chunk_len_buf, &pngp, 4);

    //dbg_printbuffer(chunk_len_buf, 4);
    
    //first 4 bytes are the length of data section
    long chunk_len = four_bytes_to_int(chunk_len_buf);

    accum_png_len += chunk_len + 4 + 4 + 4; // plus len, crc, label
    DEBUG_PRINT(("at %lu --> %lld\n", accum_png_len, PNG_LENGTH));

    if (accum_png_len >= PNG_LENGTH)
      break;

    //read the chunk label (name of this header)
    DEBUG_PRINT(("Reading chunk label\n"));
    buf_read(chunk_label, &pngp, 4);

    chunk_count += 1;

    if (memcmp(chunk_label, "IDAT", 4) == 0) {

      DEBUG_PRINT(("Chunk %d is IDAT\n", chunk_count));

      zipped_idats_len += chunk_len;

      unsigned char *in_chunk_bytes = calloc(chunk_len, 1);
      bzero(in_chunk_bytes, chunk_len);
      buf_read(in_chunk_bytes, &pngp, chunk_len);

      unzipped_idats_buf = realloc(unzipped_idats_buf, unzip_buf_len);

      inflate_stream.next_in = in_chunk_bytes; //tell inflater its input buffer
      inflate_stream.avail_in = chunk_len; //tell inflater its input size

      
      do {  //uncompress this idat

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

          print_error_html(PROCESS_ERROR);
          free(in_chunk_bytes);
          free(unzipped_idats_buf);
          free(ENTIRE_PNG_BUF);
          return;
        }
      } while (inflate_stream.avail_in != 0);

      free(in_chunk_bytes);
      pngp += 4; // skip this idat's CRC

    } else {
      DEBUG_PRINT(("Chunk %d not IDAT:\n", chunk_count));
      dbg_printbuffer(chunk_label, 4);
      //pngp += chunk_len + 4; //skip chunk and crc
      DEBUG_PRINT(("anc: %lld\n", ancil_chunks_len));
      
      ancil_chunks_buf = realloc(ancil_chunks_buf, ancil_chunks_len + 4 + 4 + chunk_len + 4);
      append_bytes(ancil_chunks_buf, chunk_len_buf, &ancil_chunks_len, 4);
      append_bytes(ancil_chunks_buf, chunk_label, &ancil_chunks_len, 4);

      unsigned char *chunk_data = malloc(chunk_len);
      buf_read(chunk_data, &pngp, chunk_len);

      append_bytes(ancil_chunks_buf, chunk_data, &ancil_chunks_len, chunk_len );

      unsigned char chunk_crc_buf[4];
      buf_read(chunk_crc_buf, &pngp, 4);

      append_bytes(ancil_chunks_buf, chunk_crc_buf, &ancil_chunks_len, 4);

      DEBUG_PRINT(("anc: %lld\n", ancil_chunks_len));

      free(chunk_data);

      //pngp += chunk_len ;
      
    }
  }

  
  dbg_printbuffer(ancil_chunks_buf, ancil_chunks_len);

  unsigned long unzipped_idats_len = inflate_stream.total_out; 
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
        glitch_random(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len, 0.0005);
        break;
      case 6:
        glitch_random_filter(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len);
        break;
      default:
        glitch_filter(unzipped_idats_buf, unzipped_idats_len, ihdr_infos.scanline_len, g);
    }

    long long glitched_idats_len = 0;
    unsigned char *glitched_idats= zip_idats(unzipped_idats_buf, unzipped_idats_len, &glitched_idats_len);

    if (glitched_idats == NULL) {
      free (unzipped_idats_buf);
      return;
    }

    //Now write thing to file:
    // -Sig
    // -IHDR
    // -[optional: others?]
    // -Idats
    // -IEND


    char* path = out_file_paths + (g * path_max_len);

    snprintf(path, path_max_len, "%s/%s-%d.png", output_dir, infname_sans_ext, g);

    DEBUG_PRINT(("Output file name is %s\n", path));

    FILE *outfp = fopen(path, "wb");

    write_glitched_image(glitched_idats, glitched_idats_len, ihdr_bytes_buf, ancil_chunks_buf, ancil_chunks_len, outfp);

    fclose(outfp);
    free(glitched_idats);
  }

  free(ancil_chunks_buf);

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

    if (content_length <= 0) {
      switch (content_length) {
        case -1: print_error_html(UPLOAD_ERROR); break;
        case -2: print_error_html("The uploaded file is too big! Maximum file size is 10MB"); break;
        case -3: print_error_html("The file was too small! Please upload a valid PNG file!"); break;
        default: print_error_html(UPLOAD_ERROR); 
      }
      continue;
    }

    char *form_boundary_buf = malloc(MAX_FORM_BOUNDARY_LENGTH);
    bzero(form_boundary_buf, MAX_FORM_BOUNDARY_LENGTH);

    int form_boundary_buf_len = get_form_boundary(form_boundary_buf);

    if (form_boundary_buf_len <= 0) {
      print_error_html("Error processing form upload");
      free(form_boundary_buf);
      continue;
    }

    form_boundary_buf = realloc(form_boundary_buf, form_boundary_buf_len);

    char* form_meta_buf = calloc(MAX_FORM_META_LENGTH, 1);
    bzero(form_meta_buf, MAX_FORM_META_LENGTH);
    int form_meta_buf_sz  = get_form_meta_buf(form_meta_buf);

    //dbg_printbuffer((unsigned char*)form_meta_buf, form_meta_buf_sz);

    if (form_meta_buf_sz <= 0) {
      free(form_boundary_buf);
      free(form_meta_buf);
      print_error_html("Error processing form upload!");
      continue;
    }

    form_meta_buf = realloc(form_meta_buf, form_meta_buf_sz+1);
    form_meta_buf[form_meta_buf_sz] = '\0';

    char *form_filename_buf = malloc(MAX_FILENAME_LENGTH);
    bzero(form_filename_buf, MAX_FILENAME_LENGTH);

    char *form_filename = 
      get_form_filename(form_meta_buf, form_filename_buf);

    free(form_meta_buf);

    if (form_filename == NULL) {
      free(form_filename_buf);
      free(form_boundary_buf);
      print_error_html("Error processing form upload");
      continue;
    }

    unsigned char *png_buf = calloc(content_length, 1);

    PNG_LENGTH = get_uploaded_file_buf(png_buf, content_length,
        form_boundary_buf, form_boundary_buf_len);

    free(form_boundary_buf);
    DEBUG_PRINT(("Size of uploaded png: %ld\n", PNG_LENGTH));

    if (PNG_LENGTH <= 0) {
      free(form_filename_buf);
      free(png_buf);
      print_error_html("Error processing form upload");
      continue;
    }

    png_buf = realloc(png_buf, PNG_LENGTH);

    begin(form_filename, png_buf, PNG_LENGTH);

    free(form_filename_buf);
  }

  //To properly free memory
  OS_LibShutdown();

  free(success_template);
  free(error_template);

  return 0;
}
