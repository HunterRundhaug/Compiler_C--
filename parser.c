
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "symtab.h"

// Prototypes //
int parse(void);
void prog();
void var_decl();
void id_list();
void type();
void func_defn();
void opt_formals();
void formals();
void opt_var_decls();
void opt_stmt_list();
void stmt();
void fn_call();
void opt_expr_list();
void match(int tok);
void printStandardError(int,int);
// ----------- //

int curTok;
int nextTok;
int thirdTok;

int curLine;
int nextTokLine;
int thirdTokLine;

int lastTypeDeclared;  // used for id_list variable declaration of multiple variables.

char* curLexeme;
char* nextLexeme;
char* thirdLexeme;

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
    if (curTok == kwINT && nextTok == ID) {
        if(thirdTok == LPAREN){
            func_defn();
        }
        else{
            var_decl();
        }
        prog();
    }
    else if(curTok != EOF){
        printStandardError(9, curTok);
        exit(1);
    }
    // else epsilon (do nothing)
}

void var_decl(){
    type();
    id_list();
    match(SEMI);
}

void id_list(){
    add_new_symbol(curLexeme, SYM_INT_VAR);
    match(ID);
    if(curTok == COMMA){
        match(COMMA);
        id_list();
    }
}

void type(){
    lastTypeDeclared = curTok;
    match(kwINT);
}

void func_defn(){
    type();
    add_new_symbol(curLexeme, SYM_FUNC);
    match(ID);
    add_new_scope(); // Add new scope because we are in a new function
    match(LPAREN);
    opt_formals();
    match(RPAREN);
    match(LBRACE);
    opt_var_decls();
    opt_stmt_list();
    match(RBRACE);
    pop_current_scope(); // leave this functions scope
}

void opt_formals(){
    if(curTok == kwINT){
        formals();
    }
    // empty
}

void formals(){
    type();
    add_new_symbol(curLexeme, SYM_INT_VAR);
    match(ID);
    if(curTok == COMMA){
        match(COMMA);
        formals();
    }
}

void opt_var_decls(){
    if(curTok == kwINT){
        var_decl();
        opt_var_decls();
    }
    // or epsilon
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
    //printf("cur:%s, expected:%s\n", token_name[curTok], token_name[tok]);
    if (curTok == tok) {
        curTok = nextTok; // advance to next token
        curLine = nextTokLine;
        curLexeme = nextLexeme;

        nextTok = thirdTok;
        nextTokLine = thirdTokLine;
        nextLexeme = thirdLexeme;

        thirdTok = get_token();
        thirdTokLine = getLineNumber();
        thirdLexeme = get_lexeme();
    } else {
        printStandardError(tok, curTok);
        exit(1);
    }
}

void printStandardError(int expected, int actual){
    fprintf(stderr, "ERROR LINE %d, exit status = 1, Expected {%s} But Got {%s}\n", curLine, token_name[expected], token_name[actual]);
}

void increment_tokens(){
    curTok = get_token();
    curLine = getLineNumber();
    curLexeme = get_lexeme();

    nextTok = get_token();
    nextTokLine = getLineNumber();
    nextLexeme = get_lexeme();
    
    thirdTok = get_token();
    thirdTokLine = getLineNumber();
    thirdLexeme = get_lexeme();
}

// Main parse function
int parse(void) {
    add_new_scope(); // Append Global scope to symbol table
    increment_tokens();
    prog();
    return 0;
}