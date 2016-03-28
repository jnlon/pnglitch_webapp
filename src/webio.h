#ifndef WEBIO_H_
#define WEBIO_H_

#define SUCCESS_FILE_PATH "success.template.html"
#define ERROR_FILE_PATH "error.template.html"

#define UPLOAD_ERROR "Error while processing file upload!"
#define GLITCH_ERROR "Error while glitching your PNG file!"
#define BUSY_ERROR "pnglitch is too busy right now! Try again in a minute!"

#define MAX_FORM_BOUNDARY_LENGTH 1000  //boundary delimits file content
#define MAX_FORM_META_LENGTH 1024*10    //multipart/form-data stuff
#define MAX_FILENAME_LENGTH 50        //larger than this will be truncated
#define MAX_PATH_LENGTH 150         //includes filename and directory path
#define NUM_OUTPUT_FILES 7

/*Templates will be malloced in load_html_templates
These templates will contain format 
strings and will be passed to printf*/
char* success_template;
char* error_template;

long long get_dir_bytesize(char* outdir);
long get_content_length();
long get_uploaded_file_buf(unsigned char *buf, long content_length);

void my_fcgi_setup();
int skip_until_end_of_form();

char *get_random_filename(char *form_filename, int length);

void *thread_delete_files(void *paths);
char *load_html_template(char *path);
void print_error_html(char *msg);
void print_success_html(char *g1, char *g2, char *g3, char *g4, char *g5, char *g6, char *g7);

#endif 
