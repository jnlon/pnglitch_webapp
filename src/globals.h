#ifndef GLOBALS_H_
#define GLOBALS_H_
#define MAX_PNG_IN_BYTESIZE 10737418240 //10MB
#define MAX_PNG_OUT_BYTESIZE 32212254720 //30MB

#define OUTPUT_DIRECTORY "pnglitch_c_output"
#define MAX_USER_THREADS 10

//Need to be global for custom libpng write/read fns
extern long long MY_PNG_READ_OFFSET;
extern unsigned char *ENTIRE_PNG_BUF;
extern long long PNG_LENGTH; //Also used for writing
extern pthread_mutex_t mutextcount;
extern int tcount;

#endif
