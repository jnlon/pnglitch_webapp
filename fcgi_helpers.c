#include "fcgi_helpers.h"
#include <fcgi_stdio.h>
#include <fastcgi.h>
#include <string.h>
#include <stdlib.h>

int get_form_boundary(char* boundary) {

  //Look until first newline, this is the content boundary
  int bi = 0;
  while (1) {
    boundary[bi] = getc(stdin);
    if (boundary[bi] == '\n')
      break;
    bi++;
  }
  return bi;
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
      return -1;
    }
    i++;
  }

  return i;
}

int get_form_filename(char* buf, char* filename) {

  char* fname_begin = strstr(buf, "filename=") + 10;

  if (fname_begin == NULL)
    return -1;

  char* fname_end = strstr(fname_begin, "\"");
  if (fname_end == NULL)
    return -1;

  int i=0;

  while (i < MAX_FILENAME_LENGTH && fname_begin != fname_end) {
    filename[i] = *fname_begin;
    fname_begin++;
    i++;
  }

  return i;
}
