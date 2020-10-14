output: main.o heap.o custom_unistd.o
	gcc -g main.o heap.o custom_unistd.o -o output

main.o: main.c
	gcc -g -c main.c

heap.o: heap.c
	gcc -g -c heap.c

custom_unistd.o: custom_unistd.c
	gcc -g -c custom_unistd.c

clean:
	rm *.o output
