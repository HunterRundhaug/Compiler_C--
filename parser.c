#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"
#include "symtab.h"
#include "ast.h"          
#include "codegen.h"    

/* prototypes for AST helpers (implemented in ast.c) */
extern void *make_func_def(char *name, int nargs, char **args, void *body);
extern void *make_func_call(char *callee, void *args);
extern void *make_stmt_list(void *head, void *rest);
extern void *make_expr_list(void *head, void *rest);
extern void *make_identifier(char *name);
extern void *make_intconst(int val);
extern void *make_expr(NodeType op_type, void *lhs, void *rhs);
extern void *make_assg(char *lhs, void *rhs);
extern void *make_if(void *expr, void *thenp, void *elsep);
extern void *make_while(void *expr, void *body);
extern void *make_return(void *expr);

/* getter prototypes from ast.c */
extern NodeType ast_node_type(void *ptr);
extern void *expr_list_head(void *ptr);
extern void *expr_list_rest(void *ptr);
extern char *expr_id_name(void *ptr);

/* Prototypes -- updated to return AST nodes (void *) where appropriate */
int parse(void);
void prog();
void var_decl();
void id_list();
void type();
void *func_defn();
void *opt_formals();
void *formals();
void opt_var_decls();
void *opt_stmt_list();
void *stmt();
int might_be_statement();
void *if_stmt();
void *expr_list();
void *bool_exp();
void *arith_exp();
NodeType relop();
void *while_stmt();
void *return_stmt();
void *assg_stmt();
void *fn_call();
void *opt_expr_list();
void match(int tok);
void printStandardError(int,int);
void increment_tokens();

/* ----------- */

extern int chk_decl_flag;
extern int print_ast_flag; 
extern int gen_code_flag; 

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

Symbol* lastSymbolAdded;
int optExpListCount;

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

/* ------------------ parser implementation ------------------ */

/* prog: processes top-level declarations (functions or globals) */
void prog() {
    if (curTok == kwINT) {
        if(nextTok == ID){
            if(thirdTok == LPAREN){
                void *fnode = func_defn();
                (void)fnode;
            } else {
                var_decl();
            }
            prog();
        } else {
            match(kwINT);
            printStandardError(9, curTok);
            exit(1);
        }
    } else if(curTok != EOF){
        printStandardError(9, curTok);
        exit(1);
    }
}

/* var_decl: no AST produced for global var decls */
void var_decl(){
    type();
    id_list();
    match(SEMI);
}

/* id_list: adds variable(s) to current scope */
void id_list(){
    if( chk_decl_flag == 1 && lookup_in_current_scope(curLexeme, SYM_INT_VAR) == 0){
        fprintf(stderr, "ERROR LINE %d: INT variable %s was ALREADY defined in the current scope\n", curLine, curLexeme);
        exit(1);
    }
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

/* func_defn: build AST for the function */
void *func_defn(){
    type();
    if( chk_decl_flag == 1 && lookup_global_scope(curLexeme, SYM_FUNC) == 0){
        fprintf(stderr, "ERROR LINE %d: Function %s was ALREADY defined in the global scope\n", curLine, curLexeme);
        exit(1);
    }

    lastSymbolAdded = add_new_symbol(curLexeme, SYM_FUNC);
    char *funcName = strdup(curLexeme);
    match(ID);

    add_new_scope();
    match(LPAREN);
    void *formals_list = opt_formals();
    match(RPAREN);

    match(LBRACE);
    opt_var_decls();
    void *body = opt_stmt_list();
    match(RBRACE);

    int nargs = 0;
    char **args_arr = NULL;
    if (formals_list != NULL) {
        void *cur = formals_list;
        while (cur != NULL) {
            nargs++;
            cur = expr_list_rest(cur);
        }
        args_arr = malloc(sizeof(char*) * nargs);
        int idx = 0;
        cur = formals_list;
        while (cur != NULL) {
            void *hd = expr_list_head(cur);
            args_arr[idx++] = strdup(expr_id_name(hd));
            cur = expr_list_rest(cur);
        }
    }

    void *func_ast = make_func_def(funcName, nargs, args_arr, body);

    if (print_ast_flag) {
        print_ast(func_ast);
    }

     if (gen_code_flag) {
        codegen_init_once();      /* prints the println runtime once */
        codegen_func(func_ast);   /* AST -> TAC -> MIPS for this function */
    }

    pop_current_scope();
    return func_ast;

    pop_current_scope();
    return func_ast;
}

/* opt_formals: returns EXPR_LIST of IDENTIFIER nodes or NULL */
void *opt_formals(){
    if(curTok == kwINT){
        return formals();
    }
    return NULL;
}

/* formals: parse one or more formals and return EXPR_LIST */
void *formals(){
    type();
    if( chk_decl_flag == 1 && lookup_in_current_scope(curLexeme, SYM_INT_VAR) == 0){
        fprintf(stderr, "ERROR LINE %d: INT variable %s was ALREADY defined in the current scope\n", curLine, curLexeme);
        exit(1);
    }

    add_new_symbol(curLexeme, SYM_INT_VAR);
    append_child_to_symbol(lastSymbolAdded, curLexeme, SYM_INT_VAR);

    char *paramName = strdup(curLexeme);
    void *idnode = make_identifier(paramName);
    match(ID);

    if(curTok == COMMA){
        match(COMMA);
        void *rest = formals();
        return make_expr_list(idnode, rest);
    } else {
        return make_expr_list(idnode, NULL);
    }
}

/* opt_var_decls: parse local variable declarations (no AST) */
void opt_var_decls(){
    if(curTok == kwINT){
        var_decl();
        opt_var_decls();
    }
}

/* opt_stmt_list: returns STMT_LIST (possibly NULL) */
void *opt_stmt_list() {
    if (might_be_statement() == 0) {
        void *s = stmt();
        void *rest = opt_stmt_list();
        /* Preserve explicit empty statements as list nodes */
        if (s == NULL) {
            return make_stmt_list(NULL, rest);
        }
        return make_stmt_list(s, rest);
    }
    return NULL;
}

int might_be_statement(){
    if(curTok==ID||curTok==kwWHILE||curTok==kwIF||curTok==kwRETURN||curTok==LBRACE||curTok==SEMI)
        return 0;
    return 1;
}

/* stmt: returns AST for the statement; NULL for empty ';' */
void *stmt(){
    if(curTok == ID){
        if(nextTok == opASSG){
            return assg_stmt();
        } else {
            void *call = fn_call();
            match(SEMI);
            return call;
        }
    }
    else if(curTok == kwWHILE){
        return while_stmt();
    }
    else if(curTok == kwIF){
        return if_stmt();
    }
    else if(curTok == kwRETURN){
        return return_stmt();
    }
    else if (curTok == LBRACE) {
    match(LBRACE);
    void *body = opt_stmt_list();
    match(RBRACE);
    // If the block is empty, don't fabricate a stmt_list node.
    // Let the parent (e.g., while/if/function body) decide how to print braces.
    return body;  // return NULL if empty, otherwise the inner list
}
    else if(curTok == SEMI){
        match(SEMI);
        return NULL;
    }
    else if(chk_decl_flag == 1){
        printStandardError(kwIF, curTok);
        fprintf(stderr, "ERROR LINE %d: Expected statement after 'if' or 'else'\n", curLine);
        exit(1);
    }
    return NULL;
}

/* if_stmt: returns IF AST node */
void *if_stmt(){
    match(kwIF);
    match(LPAREN);
    void *cond = bool_exp();
    match(RPAREN);
    void *thenp = stmt();
    void *elsep = NULL;
    if(curTok == kwELSE){
        match(kwELSE);
        elsep = stmt();
    }
    return make_if(cond, thenp, elsep);
}

/* while_stmt: returns WHILE AST node */
void *while_stmt(){
    match(kwWHILE);
    match(LPAREN);
    void *cond = bool_exp();
    match(RPAREN);
    void *body = stmt();
    return make_while(cond, body);
}

/* return_stmt: returns RETURN AST node */
void *return_stmt(){
    match(kwRETURN);
    if(curTok == SEMI){
        match(SEMI);
        return make_return(NULL);
    } else {
        void *expr = arith_exp();
        match(SEMI);
        return make_return(expr);
    }
}

/* assg_stmt: returns ASSG AST node */
void *assg_stmt(){
    char *lhs = strdup(curLexeme);
    match(ID);
    match(opASSG);
    void *rhs = arith_exp();
    match(SEMI);
    return make_assg(lhs, rhs);
}

/* fn_call: returns FUNC_CALL AST node, performs semantic checks */
void *fn_call(){
    optExpListCount = 0;
    int retVal = lookup_local_to_global(curLexeme, SYM_FUNC);
    if(chk_decl_flag == 1){
        if(retVal == -1){ /* function never declared */
            fprintf(stderr, "ERROR LINE %d: Function %s was never defined\n", curLine, curLexeme);
            exit(1);
        }
        else if(retVal == -2){ /* found a definition of same ID but not a function */
            fprintf(stderr, "ERROR LINE %d: Function %s was defined as another type or something.\n", curLine, curLexeme);
            exit(1);
        }
    }

    char *callee = strdup(curLexeme);
    match(ID);
    match(LPAREN);
    void *args = opt_expr_list(); /* EXPR_LIST or NULL */

    if(chk_decl_flag == 1){
        if(optExpListCount != retVal){
            fprintf(stderr, "ERROR LINE %d: Function was called with incorrect number of arguments.\n", curLine);
            fprintf(stderr, "exprList:%d, retVal:%d \n", optExpListCount, retVal);
            exit(1);
        }
    }
    match(RPAREN);

    return make_func_call(callee, args);
}

/* opt_expr_list: returns EXPR_LIST or NULL */
void *opt_expr_list(){
    if(curTok == ID || curTok == INTCON){
        return expr_list();
    }
    return NULL;
}

/* expr_list: builds EXPR_LIST AST and increments optExpListCount */
void *expr_list(){
    optExpListCount++;
    void *head = arith_exp();
    if(curTok == COMMA){
        match(COMMA);
        void *rest = expr_list();
        return make_expr_list(head, rest);
    } else {
        return make_expr_list(head, NULL);
    }
}

/* bool_exp: a relational expression -> returns Expr AST (EQ/NE/GT/...) */
void *bool_exp(){
    void *lhs = arith_exp();
    NodeType op = relop();
    void *rhs = arith_exp();
    return make_expr(op, lhs, rhs);
}

/* arith_exp: returns IDENTIFIER or INTCONST AST node (G2 arithmetic is simple) */
void *arith_exp() {
    if(curTok == ID) {
        /* semantic checking: ensure ID is a variable, not a function */
        if (chk_decl_flag == 1) {
            int retVal = lookup_local_to_global(curLexeme, SYM_INT_VAR);
            if (retVal == -1) {
                fprintf(stderr, "ERROR LINE %d: Variable %s was never declared.\n", curLine, curLexeme);
                exit(1);
            }
            if (retVal == -2) {
                fprintf(stderr, "ERROR LINE %d: %s is a function, not a variable.\n", curLine, curLexeme);
                exit(1);
            }
        }
        char *name = strdup(curLexeme);
        match(ID);
        return make_identifier(name);
    }
    else if (curTok == INTCON) {
        int val = atoi(curLexeme);
        match(INTCON);
        return make_intconst(val);
    }
    else {
        /* token not expected for arith_exp according to grammar */
        printStandardError(ID, curTok);
        exit(1);
    }
}

/* relop: returns NodeType corresponding to relational operator */
NodeType relop(){
    if(curTok == opEQ){
        match(opEQ);
        return EQ;
    }
    else if(curTok == opNE){
        match(opNE);
        return NE;
    }
    else if(curTok == opLE){
        match(opLE);
        return LE;
    }
    else if(curTok == opLT){
        match(opLT);
        return LT;
    }
    else if(curTok == opGE){
        match(opGE);
        return GE;
    }
    else if(curTok == opGT){
        match(opGT);
        return GT;
    }
    else{
        printStandardError(opEQ, curTok);
        exit(1);
    }
}

/* match, error helpers, and token initialization */
void match(int tok){
    if (curTok == tok) {
        curTok = nextTok;
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

static void predeclare_builtins(void) {
    // Pretend we saw: int println(int);
    Symbol *f = add_new_symbol("println", SYM_FUNC);
    if (f) {
        // one int parameter (name doesn't matter)
        append_child_to_symbol(f, "_x", SYM_INT_VAR);
    }
}

/* Main parse function */
int parse(void) {
    lastSymbolAdded = NULL;
    add_new_scope(); /* Append Global scope to symbol table */
    predeclare_builtins();
    increment_tokens();
    prog();
    return 0;
}
