#ifndef WEB_H_
#define WEB_H_

#define SUCCESS_FILE_PATH "success.template.html"
#define ERROR_FILE_PATH "error.template.html"

#define UPLOAD_ERROR "An error was encountered while processing the file upload!"
#define PROCESS_ERROR "An error was ecountered while glitching your PNG file!"

/*Templates will be malloced in load_html_templates
These templates will contain format 
strings and will be passed to printf*/

char* success_template;
char* error_template;

char* load_html_template(char *path);
void print_error_html(char *msg);
void print_success_html(char *g1, char *g2, char *g3, char *g4, char *g5, char *g6, char *g7);

#endif 
