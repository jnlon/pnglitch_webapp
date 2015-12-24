_:
	gcc -Wall -g -std=gnu11 main.c debug.c bufs.c webio.c libs.c pnglitch.c -lpng -lz -lfcgi -pthread -o main 
debug:
	gcc -Wall -g -std=gnu11 -DDEBUG main.c debug.c bufs.c webio.c libs.c pnglitch.c -lpng -lz -lfcgi -pthread -o main 
