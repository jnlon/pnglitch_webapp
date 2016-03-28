#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <pthread.h>
#include <fcgi_stdio.h>

#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#include "webio.h"
#include "debug.h"
#include "globals.h"

long long get_dir_bytesize(char* outdir) {

  DIR *root = opendir(outdir);

  long long dir_total_size = 0;

  while (1) {
    struct dirent *entry;
    entry = readdir(root);

    if (entry == NULL)
      break;

    struct stat filestat;

    char path[MAX_PATH_LENGTH];
    snprintf(path, MAX_PATH_LENGTH, "%s/%s", OUTPUT_DIRECTORY, entry->d_name);

    int ret = stat(path, &filestat);

    if (ret == -1)  {
      DEBUG_PRINT(("stat error: %s\n", strerror(errno)));
      errno = 0;
      continue;
    }

    //not regular file
    if (!S_ISREG(filestat.st_mode))
      continue;

    long filesize = filestat.st_size;

    //DEBUG_PRINT(("%s : %ld\n", path, filesize));

    dir_total_size += filesize;
  }

  DEBUG_PRINT(("'%s' dir size: %lld\n", outdir, dir_total_size));
  fflush(stdout);
  closedir(root);

  return dir_total_size;
}


char *get_random_filename(char *form_filename, int length) {
  for (int i=0;i<length;i++) {
    char r = ((rand()%(57 - 49) ) + 49);
    form_filename[i] = r;
  }
  return form_filename;
}

void my_fcgi_setup() {

  //call accept() otherwise we won't recieve error output if
  //something goes wrong before main loop
  //
  //Don't call it if debugging, otherwise we can't test it
#ifndef DEBUG
  FCGI_Accept();
#endif

  success_template = load_html_template(SUCCESS_FILE_PATH);
  error_template = load_html_template(ERROR_FILE_PATH);

  if (success_template == NULL || error_template == NULL) 
    error_fatal(-1, "load_html_template", "Cannot load HTML template file(s)");
}

void *thread_delete_files(void *paths) {

  DEBUG_PRINT(("going to sleep on thread\n"));

  usleep(TIME_BEFORE_DELETION);

  for (int i=0;i<NUM_OUTPUT_FILES;i++) {
    //DEBUG_PRINT(("%d %d:\n" , i, i*MAX_PATH_LENGTH));
    char *path = ((char*)paths) + (i*MAX_PATH_LENGTH);
    DEBUG_PRINT(("deleting path %s\n", path));
    remove(path);
  }

  free(paths);

  pthread_exit(NULL);
}

char *load_html_template(char *path) {

  FILE *template_fp = fopen(path, "rb");

  if (template_fp == NULL) {
    printf("Cannot load template file '%s'\n", path);
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

int skip_until_end_of_form() {

  int form_length = 0;

  while (1) {
    if (getchar() == '\r' && getchar() == '\n' &&
        getchar() == '\r' && getchar() == '\n')
    {
      DEBUG_PRINT(("Found end of upload form in %d bytes\n", form_length));
      return form_length;
    }

    if (form_length >= MAX_FORM_BOUNDARY_LENGTH) {
      DEBUG_PRINT(("Form boundary is too large!\n"));
      return -1;
    }

    form_length += 1;
  }
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

long get_uploaded_file_buf(unsigned char *upload, long content_length) {
  long i = 0;
  for (i=0;i<content_length;i++) {
    char x = getchar();
    if (feof(stdin))
      break;
    upload[i] = x;
  }
  return i;
}
