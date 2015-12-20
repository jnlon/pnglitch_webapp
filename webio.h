#ifndef WEBIO_H_
#define WEBIO_H_

#define SUCCESS_FILE_PATH "success.template.html"
#define ERROR_FILE_PATH "error.template.html"

#define UPLOAD_ERROR "An error was encountered while processing the file upload!"
#define UPLOAD_ERROR_TOO_BIG "An error was encountered while processing the file upload!"
#define PROCESS_ERROR "An error was ecountered while glitching your PNG file!"

#define MAX_FORM_BOUNDARY_LENGTH 1000  //boundary delimits file content
#define MAX_FORM_META_LENGTH 10000    //multipart/form-data stuff
#define MAX_CONTENT_LENGTH 10485760L   //10MB
#define MAX_FILENAME_LENGTH 50        //larger than this will be truncated
#define MAX_PATH_LENGTH 100         //includes filename and directory path
#define NUM_OF_OUTPUT_FILES 7

#define TIME_BEFORE_DELETION 4*1000*1000

/*Templates will be malloced in load_html_templates
These templates will contain format 
strings and will be passed to printf*/
char* success_template;
char* error_template;

int get_form_boundary(char* boundary);
int get_form_meta_buf(char* buf);
char *get_form_filename(char* buf, char* filename);
long get_content_length();
long get_uploaded_file_buf(unsigned char *buf, long content_length, 
    char *form_boundary, int form_boundary_len);


void *thread_delete_files(void *paths);


char* load_html_template(char *path);
void print_error_html(char *msg);
void print_success_html(char *g1, char *g2, char *g3, char *g4, char *g5, char *g6, char *g7);

#endif 
