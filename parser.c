
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
void printStandardError();
// ----------- //

int curTok;

void prog(){
    while(curTok == kwINT){
        func_defn();
    }
    if(curTok == -1){
        return;
    }
    printStandardError();
    exit(1);
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

void opt_stmt_list(){
    while(curTok == ID || curTok == LPAREN || curTok == RPAREN){
        stmt();
    }
}

void stmt(){
    fn_call();
    match(SEMI);
}

void fn_call(){
    if( curTok != ID && curTok != LPAREN && curTok != RPAREN){
        printStandardError();
        exit(1);
    }
}

void opt_expr_list(){
    // empty
}

void match(int tok){
    if (curTok == tok) {
        curTok = get_token(); // advance to next token
    } else {
        printStandardError();
        exit(1);
    }
}

void printStandardError(){
    fprintf(stderr, "ERROR LINE %d, exit status = 1\n", getLineNumber());
}

// Main parse function
int parse(void) {
    curTok = get_token();
    prog();
    return 0;
}