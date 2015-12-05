#ifndef FCGI_H_
#define FCGI_H_

#define MAX_FORM_BOUNDARY_LENGTH 500
#define MAX_FORM_META_LENGTH 10000
#define MAX_CONTENT_LENGTH 10485760
#define MAX_FILENAME_LENGTH 100

int get_form_boundary(char* boundary);
int get_form_meta_buf(char* buf);
int get_form_filename(char* buf, char* filename);
long get_content_length();
unsigned long get_uploaded_file_buf(unsigned char *buf, long content_length, 
    char *form_boundary, int form_boundary_len);

#endif 


