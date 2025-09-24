#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdio.h>

// Struct definitions (if not already included elsewhere)
typedef struct Symbol {
    char* name;
    int type;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    struct Scope* next;
    Symbol* symbols;
} Scope;

// Global scope stack head
extern Scope* table_head;

// Scope management
void add_new_scope();               // Pushes a new scope onto the stack
void add_new_symbol(char*, int);    // Pushes a new symbol to the symbol list of the current scope (head of scope)
void pop_current_scope();           // Pops current scope and frees contents
void free_scope(Scope* scope);      // Frees all symbols in a given scope

#endif