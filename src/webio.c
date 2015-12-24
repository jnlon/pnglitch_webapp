#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <pthread.h>
#include <fcgi_stdio.h>

#include "webio.h"
#include "debug.h"
#include "globals.h"

void *thread_delete_files(void *paths) {

  tcount += 1;
  pthread_mutex_unlock(&mutextcount);

  DEBUG_PRINT(("SLEEPING:\n"));

  usleep(TIME_BEFORE_DELETION);

  for (int i=0;i<NUM_OUTPUT_FILES;i++) {
    //DEBUG_PRINT(("%d %d:\n" , i, i*MAX_PATH_LENGTH));
    char *path = ((char*)paths) + (i*MAX_PATH_LENGTH);
    DEBUG_PRINT(("deleting path %s\n", path));
    remove(path);
  }

  free(paths);

  pthread_mutex_lock(&mutextcount);
  tcount -= 1;
  pthread_mutex_unlock(&mutextcount);

  pthread_exit(NULL);
}

char *load_html_template(char *path) {

  FILE *template_fp = fopen(path, "rb");

  if (template_fp == NULL) {
    printf("Cannot load template file '%s'\n", path);
    fprintf(stderr, "Cannot load template file '%s'\n", path);
    fflush(NULL);
    return NULL;
  }

  const int read_sz = 4096;

  char *template_buf = calloc(read_sz, 1);
  long template_sz = read_sz;


    //error_fatal(-1, "load_html_template", "fopen failed on template file");

  while (1) {

    int read = fread(template_buf, 1, read_sz, template_fp);

    if (read == 0)
      break;
    
    template_sz += read;
    template_buf = realloc(template_buf, template_sz);
  }

  fclose(template_fp);
  template_buf[template_sz-1] = '\0';

  return template_buf;

}

//TODO: how many args?
void print_success_html(char* g1, char* g2, char* g3, char* g4, char* g5, char* g6, char* g7) {
  printf(success_template, g1, g2, g3, g4, g5, g6, g7);
}

void print_error_html(char* msg) {
  printf(error_template, msg);
}


int get_form_boundary(char* boundary) {

  //Everything until \r\n (inclusive) is part of the boundary
  int bi = 0;
  while (1) {
    boundary[bi] = getchar();
    if (bi >= 1 && boundary[bi] == '\n' && boundary[bi-1] == '\r') {
      DEBUG_PRINT(("form boundary is %d bytes long\n", bi));
      break;
    }

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

    unsigned char byte = getchar();

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
    char x = getchar();
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

  //put it in filename
  while (i < MAX_FILENAME_LENGTH && fname_begin != fname_end) {
    filename[i] = *fname_begin;
    fname_begin++;
    i++;
  }

  char *ext_dot = strrchr(filename, '.');

  //DEBUG_PRINT(("fn: %d %p\n", filename, filename));
  //DEBUG_PRINT(("ext: %d %p\n", ext_dot, ext_dot));

  //No filename or begins with dot
  if (ext_dot == NULL || filename == ext_dot)
    filename[i] = '\0'; //make end of form end of string
  else 
    ext_dot[0] = '\0'; //make dot end of string


  //Note: also set directory permissions
  DEBUG_PRINT(("Filename From form: %s (len %d)\n", filename, i));
  filename = basename(filename);
  DEBUG_PRINT(("Sanitized: %s\n", filename));

  return filename;
}

