#ifndef GLOBALS_H_
#define GLOBALS_H_

#define OUTPUT_DIRECTORY "pnglitch_c_output"
#define TIME_BEFORE_DELETION 90*1000*1000 //90 seconds 
#define MAX_CONTENT_LENGTH 10*1024*1024  //max upload, 10MB

// How many bytes of disk can be used at once
#define MAX_DISK_USAGE 150*1024*1024 //150MB

//Need to be global for custom libpng write/read fns
extern long long MY_PNG_READ_OFFSET;
extern unsigned char *ENTIRE_PNG_BUF;
extern long long PNG_LENGTH; //Also used for writing

#endif
