all:
	gcc -Wall -Werror src/*.c -o bank -pthread
clean:
	rm bank
