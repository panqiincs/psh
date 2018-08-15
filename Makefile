CC=gcc

psh:
	$(CC) -Wall psh.c
debug:
	$(CC) -Wall -g -DDEBUG psh.c
clean:
	rm *.out
