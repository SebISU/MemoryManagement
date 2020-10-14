output: main.o heap.o custom_unistd.o
	gcc main.o heap.o custom_unistd.o -o output

main.o: main.c
	gcc -c main.c

heap.o: heap.c
	gcc -c heap.c

custom_unistd.o: custom_unistd.c
	gcc -c custom_unistd.c

clean:
	rm *.o output
