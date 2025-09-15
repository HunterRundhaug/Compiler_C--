#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "scanner.h"

/*
    Author: Hunter Rundhaug
    CSC-473
    9/6/20205
*/

void alphaNumericParse();
int which_one();
void skip_comments();
void parse_int_constant();

char* lexeme;  /* the string corresponding to the current token */
int cur;
int curIndex;

int get_token(){
    // reset all variables // 
    cur = 0;
    curIndex = 0;


    // Skip Whitespace //
    do{
        cur = getchar();
        if(cur == EOF){
            return EOF;
        }
        else if(!isspace(cur)){ // put cur back if it was not whitespace
            ungetc(cur, stdin);
        }
    }
    while(isspace(cur));

    // Allocate Space for Lexeme //
    lexeme = (char *)calloc(4096, sizeof(char));  
    if (lexeme == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }

    // Get first char for this token //
    cur = getchar();
    lexeme[curIndex++] = cur;
    if(cur == EOF){
        return EOF;
    }

    // ID, kwINT, kwIF, kwELSE, kwWHILE, kwRETURN //
    else if(isalpha(cur)){
        alphaNumericParse();
        return which_one();
    }

    // INTCON //
    else if(isdigit(cur)){
        parse_int_constant();
        return INTCON;
    }

    // LPAREN //
    else if(cur == '('){
        return LPAREN;
    }

    // RPAREN //
    else if(cur == ')'){
        return RPAREN;
    }

    // LBRACE //
    else if(cur == '{'){
        return LBRACE;
    }

    // RBRACE //
    else if(cur == '}'){
        return RBRACE;
    }

    // COMMA //
    else if(cur == ','){
        return COMMA;
    }

    // SEMI //
    else if(cur == ';'){
        return SEMI;
    }

    // opASSG, opEQ //
    else if(cur == '='){
        cur = getchar();
        if(cur == '='){
            lexeme[curIndex++] = cur;
            return opEQ;
        }
        else{
            ungetc(cur, stdin);
            return opASSG;
        }
    }

    // opADD //
    else if(cur == '+'){
       return opADD; 
    }

    // opSUB //
    else if( cur == '-'){
        return opSUB;
    }

    // opMUL //
    else if(cur == '*'){
        return opMUL;
    }

    // opDIV, comment //
    else if(cur == '/'){
        cur = getchar();
        if(cur == '*'){
            // skip comments...
            skip_comments();
            return get_token();
        }
        else{
            ungetc(cur, stdin);
            return opDIV;
        }
    }

    // opNE, opNOT //
    else if(cur == '!'){
        cur = getchar();
        if(cur == '='){
            lexeme[curIndex++] = cur;
            return opNE;
        }
        else{
            ungetc(cur, stdin);
            return opNOT; 
        }
    }

    // opGT, opGE //
    else if(cur == '>'){
        cur = getchar();
        if(cur == '='){
            lexeme[curIndex++] = cur;
            return opGE;
        }
        else{
            ungetc(cur, stdin);
            return opGT;
        }
    }

    // opLT, opLE //
    else if(cur == '<'){
        cur = getchar();
        if(cur == '='){
            lexeme[curIndex++] = cur;
            return opLE;
        }
        else{
            ungetc(cur, stdin);
            return opLT;
        }
    }

    // opAND //
    else if(cur == '&'){
        cur = getchar();
        if(cur == '&'){
            lexeme[curIndex++] = cur;
            return opAND;
        }
        else{
            ungetc(cur, stdin);
            return UNDEF;
        }
    }

    // opOR //
    else if(cur == '|'){
        cur = getchar();
        if(cur == '|'){
            lexeme[curIndex++] = cur;
            return opOR;
        }
        else{
            ungetc(cur, stdin);
            return UNDEF;
        }
    }

    else{
        return UNDEF;
    }

}

void parse_int_constant(){
    do{
        cur = getchar();
        if(cur == EOF){
            return;
        }
        if(isspace(cur) || !isdigit(cur)){
            ungetc(cur, stdin);
            break;
        }
        lexeme[curIndex++] = cur;

    }
    while(isdigit(cur));

    lexeme[curIndex++] = '\0';
}

void skip_comments(){

    while((cur = getchar()) != EOF){
        if(cur == '*'){
            if((cur = getchar()) == '/'){
                return;
            }
        }
    }
}

int which_one(){

    if(strcmp(lexeme, "int") == 0){
        return kwINT;
    }
    else if(strcmp(lexeme, "if") == 0){
        return kwIF;
    }
    else if(strcmp(lexeme, "else") == 0){
        return kwELSE;
    }
    else if(strcmp(lexeme, "while") == 0){
        return kwWHILE;
    }
    else if(strcmp(lexeme, "return") == 0){
        return kwRETURN;
    }
    else{
        return ID;
    }
}

void alphaNumericParse(){

    do{
        cur = getchar();
        if(cur == EOF){
            return;
        }
        if(isspace(cur) || (!isalnum(cur) && cur != '_')){
            ungetc(cur, stdin);
            break;
        }
        lexeme[curIndex++] = cur;

    }
    while(isalnum(cur) || cur == '_');

    lexeme[curIndex] = '\0';
}