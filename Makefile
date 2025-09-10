scanner: scanner.c scanner-driver.c scanner.h
	gcc -Wall -g scanner.c scanner-driver.c -o scanner

clean:
	rm -f *.o scanner