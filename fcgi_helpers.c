#define _GNU_SOURCE
#include "fcgi_helpers.h"
#include <fcgi_stdio.h>
#include <fastcgi.h>
#include <string.h>
#include <stdlib.h>

//Contains filename, http-like stuff

int get_form_boundary(char* boundary) {

  //Look until first newline, this is the content boundary
  int bi = 0;
  while (1) {
    boundary[bi] = getc(stdin);
    if (boundary[bi] == '\n')
      break;

    if (bi >= MAX_FORM_BOUNDARY_LENGTH) {
      printf("Form boundary is too large");
      return -1;
    }

    bi++;
  }
  return bi;
}

long get_content_length() {

  char *CONTENT_LENGTH_C = getenv("CONTENT_LENGTH");

  if (CONTENT_LENGTH_C == NULL) {
    printf("No content_length!!\n");
    return -1;
  }

  long CONTENT_LENGTH = atol(CONTENT_LENGTH_C);

  if (CONTENT_LENGTH > MAX_CONTENT_LENGTH) {
    printf("File too big!\n");
    return -1;
  }

  if (CONTENT_LENGTH <= 0) {
    printf("File too short!\n");
    return -1;
  }

  return(CONTENT_LENGTH);

}


int get_form_meta_buf(char* buf) {

  int ri = 0, i = 0;
  int begin_request_sig[] = {13, 10, 13, 10};

  while(ri <= 3) { 

    int byte = getc(stdin);

    //putchar(byte);

    buf[i] = byte;

    if (byte == begin_request_sig[ri])
      ri++;
    else
      ri = 0;

    //Form meta stuff, should not be this long
    if (i > MAX_FORM_META_LENGTH) {
      printf("Past max meta length!\n");
      free(buf);
      return -1;
    }
    i++;
  }

  return i;
}

long get_uploaded_file_buf(unsigned char *upload, long content_length, 
    char *form_boundary, int form_boundary_len) {

  for (int i=0;i<content_length;i++) {
    char x = getc(stdin);
    if (feof(stdin))
      break;
    upload[i] = x;
  }

  //Now upload buffer contains the file as well as
  //trailing form metadata from browser

  unsigned char *end_ptr = memmem(upload, content_length, form_boundary, form_boundary_len);

  if (end_ptr == NULL) {
    printf("Cannot find end-of-form boundary\n");
    free(upload);
    return -1;
  }

  //C points to start of PNG file
  unsigned char *c = upload;
  ulong png_length = 0;

  //Now find where it ends
  while (c != end_ptr) {
    c++;
    png_length++;
  }

  return png_length;
}

int get_form_filename(char* buf, char* filename) {

  char* fname_begin = strstr(buf, "filename=") + 10;

  if (fname_begin == NULL) {
    printf("Cannot find filename= in form meta");
    return -1;
  }

  char* fname_end = strstr(fname_begin, "\"");

  if (fname_end == NULL) {
    printf("Cannot find filename end \" in form meta");
    return -1;
  }

  int i=0;

  while (i < MAX_FILENAME_LENGTH && fname_begin != fname_end) {
    filename[i] = *fname_begin;
    fname_begin++;
    i++;
  }

  return i;
}
