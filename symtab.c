
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "symtab.h"

/* File: symtab.c
 * Author: Hunter Rundhaug 
 * Purpose: The symbol table code for the CSC 453 project. 
 */

char* symbol_type_name[] = {
    "SYM_INT_VAR",
    "SYM_FUNC",
};

// Function Prototypes
Scope* get_new_scope();             // Allocates and initializes a new Scope
Symbol* get_new_symbol();           // Allocates and initializes a new Symbol

 Scope* table_head;

// Pushes a new scope onto the symbol table stack.
// Used to enter a new block or function scope during parsing.
 void add_new_scope(){
    if(table_head == NULL){
        table_head = get_new_scope();
    }
    else{
        Scope* new_scope = get_new_scope();
        Scope* temp = table_head;
        table_head = new_scope;
        table_head->next = temp;
    }
 }

// pops current scope and frees contents.
 void pop_current_scope(){
    if(table_head == NULL){
        return;
    }
    Scope* next = table_head-> next;
    free_scope(table_head);
    table_head = next;
 }

 void free_scope(Scope* scope){
    // not implemented...
 }

 void add_new_symbol(char* name, SymbolType type){
    if(table_head == NULL){
        fprintf(stderr, "ERROR: trying to add symbol to an empty table\n");
        exit(1);
    }
    Symbol* new_symbol = get_new_symbol();
    Symbol* temp = table_head->symbols;
    table_head->symbols = new_symbol;
    new_symbol->next = temp;

    new_symbol->name = strdup(name);
    new_symbol->type = type;

    printf("Lexeme: <%s> Symbol Type: <%s>\n", name, symbol_type_name[type]);
 }

/* ↓ ↓ ↓ ↓ Functions for mallocING new structs ↓ ↓ ↓ ↓ */
Scope* get_new_scope() {
    Scope* new_scope = malloc(sizeof(Scope));
    if (new_scope == NULL) {
        fprintf(stderr, "ERROR: MALLOC FAIL IN get_new_scope\n");
        exit(1);
    }
    new_scope->symbols = NULL;
    new_scope->next = NULL;
    return new_scope;
}

Symbol* get_new_symbol() {
    Symbol* new_symbol = malloc(sizeof(Symbol));
    if (new_symbol == NULL) {
        fprintf(stderr, "ERROR: MALLOC FAIL IN get_new_symbol\n");
        exit(1);
    }
    new_symbol->name = NULL;
    new_symbol->type = -1;
    new_symbol->next = NULL;
    return new_symbol;
}
/* - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ - ↑ -*/



