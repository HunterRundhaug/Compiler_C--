scanner: scanner.c scanner-driver.c scanner.h
	gcc -Wall -g scanner.c scanner-driver.c -o scanner

compile: scanner.c parser.c symtab.c driver.c scanner.h parser.h symtab.h
	gcc -Wall -g scanner.c parser.c symtab.c driver.c -o compile

clean:
	rm -f *.o scanner