# Build the scanner
scanner: scanner.c scanner-driver.c scanner.h
	gcc -Wall -g scanner.c scanner-driver.c -o scanner

# Build the compiler (includes AST and symbol table code)
compile: scanner.c parser.c symtab.c ast.c ast-print.c driver.c scanner.h parser.h symtab.h ast.h
	gcc -Wall -g scanner.c parser.c symtab.c ast.c ast-print.c driver.c -o compile

# Clean up build artifacts
clean:
	rm -f *.o scanner compile