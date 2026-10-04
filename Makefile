# write details to make the cflask program

cflask: cflask.o http-parser.o functions.o
	gcc cflask.o http-parser.o functions.o -o cflask -pthread

cflask.o: http-parser.o cflask.c
	gcc -c cflask.c -pthread

http-parser.o: http-parser.c http-parser.h
	gcc -c http-parser.c 	

functions.o: functions.c functions.h functionslist.h
	gcc -c functions.c

clean:
	rm -f *.o cflask
