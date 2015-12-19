#ifndef FCGI_H_
#define FCGI_H_

#define MAX_FORM_BOUNDARY_LENGTH 500  //boundary delimits file content
#define MAX_FORM_META_LENGTH 10000    //multipart/form-data stuff
#define MAX_CONTENT_LENGTH 10485760L   //10MB
#define MAX_FILENAME_LENGTH 50        //larger than this will be truncated

int get_form_boundary(char* boundary);
int get_form_meta_buf(char* buf);
char *get_form_filename(char* buf, char* filename);
long get_content_length();
long get_uploaded_file_buf(unsigned char *buf, long content_length, 
    char *form_boundary, int form_boundary_len);

#endif 
