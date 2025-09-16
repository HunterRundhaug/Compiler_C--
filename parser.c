
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

// Prototypes //
int parse(void);
void prog();
void type();
void func_defn();
void opt_formals();
void opt_var_decls();
void opt_stmt_list();
void stmt();
void fn_call();
void opt_expr_list();
void match(int tok);
void printStandardError(int,int);
// ----------- //

int curTok;

char* token_name[] = {
  "UNDEF",
  "ID",
  "INTCON",
  "LPAREN",
  "RPAREN",
  "LBRACE",
  "RBRACE",
  "COMMA",
  "SEMI",
  "kwINT",
  "kwIF",
  "kwELSE",
  "kwWHILE",
  "kwRETURN",
  "opASSG",
  "opADD",
  "opSUB",
  "opMUL",
  "opDIV",
  "opEQ",
  "opNE",
  "opGT",
  "opGE",
  "opLT",
  "opLE",
  "opAND",
  "opOR",
  "opNOT",
  "EOF"
};

void prog() {
    if (curTok == kwINT) {
        func_defn();
        prog();
    }
    // else epsilon (do nothing)
}

void type(){
    match(kwINT);
}

void func_defn(){
    type();
    match(ID);
    match(LPAREN);
    opt_formals();
    match(RPAREN);
    match(LBRACE);
    opt_var_decls();
    opt_stmt_list();
    match(RBRACE);
}

void opt_formals(){
    // empty
}

void opt_var_decls(){
    // empty
}

void opt_stmt_list() {
    if (curTok == ID) {
        stmt();
        opt_stmt_list();
    }
    // else epsilon (do nothing)
}

void stmt(){
    fn_call();
    match(SEMI);
}

void fn_call(){
    match(ID);
    match(LPAREN);
    opt_expr_list();
    match(RPAREN);
}


void opt_expr_list(){
    // empty
}

void match(int tok){
    //printf("Matched token: %s\n", token_name[curTok]);
    if (curTok == tok) {
        curTok = get_token(); // advance to next token
    } else {
        printStandardError(tok, curTok);
        exit(1);
    }
}

void printStandardError(int expected, int actual){
    fprintf(stderr, "ERROR LINE %d, exit status = 1, Expected {%s} But Got {%s}\n", getLineNumber(), token_name[expected], token_name[actual]);
}

// Main parse function
int parse(void) {
    curTok = get_token();
    prog();
    return 0;
}