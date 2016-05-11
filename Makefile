ChatRoom: main.c
	gcc -o chat main.c -pthread -lrt
clean:
	rm *.o project3
tar:
	tar -cf project3.tar main.c Makefile