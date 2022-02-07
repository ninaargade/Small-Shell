main:
	gcc -std=gnu99 -Wall -g -o smallsh smallsh.c
	./smallsh
	

clean:
	rm -f smallsh
