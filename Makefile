_:
	gcc -Wall -g -std=gnu11 main.c debug.c fcgi_helpers.c library_helpers.c common.c web.c -lpng -lz -lfcgi -o main 
