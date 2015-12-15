#include <stdlib.h>
#include <fcgi_stdio.h>
#include "web.h"
#include "debug.h"

char* load_html_template(char *path) {

  const int read_sz = 4096;

  char *template_buf = calloc(read_sz, 1);
  long template_sz = read_sz;

  FILE *template_fp = fopen(path, "rb");

  if (template_fp == NULL)
    error_fatal(-1, "load_html_template", "fopen failed on template file");

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

