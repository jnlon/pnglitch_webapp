_:
	#gcc -Wall -g -c -std=gnu11 -o debug.o debug.c 
	#gcc -Wall -g -c -std=gnu11 -o library_helpers.o library_helpers.c
	#gcc -Wall -g -c -std=gnu11 -o common.o common.c
	gcc -Wall -g -std=gnu11 main.c debug.c fcgi_helpers.c library_helpers.c common.c -lpng -lz -lfcgi -o main 
