_:
	gcc -Wall -g -std=gnu11 main.c debug.c bufs.c webio.c libs.c pnglitch.c -lpng -lz -lfcgi -o main 
