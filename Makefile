all:
	gcc -g -O2 swsh.c -o swsh -Wno-unused-result

clean:
	rm -f swsh
