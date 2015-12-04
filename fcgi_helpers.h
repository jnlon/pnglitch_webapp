#ifndef FCGI_H_
#define FCGI_H_

#define MAX_FORM_META_LENGTH 10000
#define MAX_CONTENT_LENGTH 10485760
#define MAX_FILENAME_LENGTH 50

int get_form_boundary(char* boundary);
int get_form_meta_buf(char* buf);
int get_form_filename(char* buf, char* filename);

#endif 


