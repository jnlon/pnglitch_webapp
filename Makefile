_:
	#gcc -Wall -g -c -std=gnu11 -o debug.o debug.c 
	#gcc -Wall -g -c -std=gnu11 -o library_helpers.o library_helpers.c
	#gcc -Wall -g -c -std=gnu11 -o common.o common.c
	gcc -Wall -g -std=gnu11 main.c debug.o library_helpers.o common.c -lpng -lz -o main 

