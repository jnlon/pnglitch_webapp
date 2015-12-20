#define _GNU_SOURCE
#include <fcgi_stdio.h>
#include <fastcgi.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

#include "fcgi_helpers.h"
#include "web.h"
#include "debug.h"

//Contains filename, http-like stuff

int get_form_boundary(char* boundary) {

  //Everything until \r\n (inclusive) is part of the boundary
  int bi = 0;
  while (1) {
    boundary[bi] = getc(stdin);
    if (bi >= 1 && boundary[bi] == '\n' && boundary[bi-1] == '\r')
      break;

    if (bi >= MAX_FORM_BOUNDARY_LENGTH) {
      DEBUG_PRINT(("Form boundary is too large"));
      return -1;
    }
    bi++;
  }
  return bi+1;
}

long get_content_length() {

  char *content_length_c = getenv("CONTENT_LENGTH");

  if (content_length_c == NULL) {
    DEBUG_PRINT(("No content_length!!\n"));
    print_error_html("Error processing form upload");
    return -1;
  }

  long content_length = atol(content_length_c);

  if (content_length > MAX_CONTENT_LENGTH) {
    DEBUG_PRINT(("content_length too big! -> %ld\n", content_length));
    return -2;
  }

  if (content_length <= 8) {
    DEBUG_PRINT(("content_length too short! -> %ld\n", content_length));
    return -3;
  }

  return(content_length);
}

int get_form_meta_buf(char* buf) {

  int sig_i = 0, i = 0;
  int begin_request_sig[] = {13, 10, 13, 10};

  while(i < MAX_FORM_META_LENGTH) { 

    if (feof(stdin)) {
      DEBUG_PRINT(("End of form (\\r\\n) not found\n"));
      return -1;
    }

    int byte = getc(stdin);

    buf[i] = byte;

    if (byte == begin_request_sig[sig_i])
      sig_i++;
    else
      sig_i = 0;

    i++;

    if (sig_i == 4) {
      DEBUG_PRINT(("Form meta length is %d\n", i));
      return i;
    }
  }

  DEBUG_PRINT(("Form upload is too big (max: %d)\n", MAX_FORM_META_LENGTH));
  return -1;
}

long get_uploaded_file_buf(unsigned char *upload, long content_length, 
    char *form_boundary, int form_boundary_len) {

  //TODO: is getc() slow reading from web server?
  int r = 0;
  for (r=0;r<content_length;r++) {
    char x = getc(stdin);
    if (feof(stdin))
      break;
    upload[r] = x;
  }

  //Now upload buffer contains the file as well as
  //trailing form metadata from browser
  unsigned char *end_ptr = memmem(upload, content_length, form_boundary, form_boundary_len);

  if (end_ptr == NULL) {
    DEBUG_PRINT(("Cannot find end-of-form boundary\n"));
    return -1;
  }

  dbg_printbuffer(upload, r);

  //C points to start of PNG file
  unsigned char *c = upload;
  ulong png_length = 0;

  //Now find where it ends
  while (c != end_ptr) {
    c++;
    png_length++;
  }


  dbg_printbuffer(upload, png_length);

  return png_length;
}

char *get_form_filename(char* buf, char* filename) {

  char *fname_begin = strstr(buf, "filename=");

  if (fname_begin == NULL) {
    DEBUG_PRINT(("Cannot find filename= in form meta"));
    return NULL;
  }

  //Now at first quote
  fname_begin += 10;

  //pointer to last quote
  char *fname_end = strchr(fname_begin, '"');

  if (fname_end == NULL) {
    DEBUG_PRINT(("Cannot find filename end \" (quote) in form meta"));
    return NULL;
  }

  int i=0;

  while (i < MAX_FILENAME_LENGTH && fname_begin != fname_end) {
    filename[i] = *fname_begin;
    fname_begin++;
    i++;
  }

  filename[i] = '\0';

  //Note: also set directory permissions
  DEBUG_PRINT(("Filename From form: %s (len %d)\n", filename, i));
  filename = basename(filename);
  DEBUG_PRINT(("Sanitized: %s\n", filename));

  return filename;
}
