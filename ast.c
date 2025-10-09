/*
 * File: ast.c
 * Author: [Your Name]
 * Purpose: Implementation of AST node structures and getter functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ============================================================= */
/*                       AST Node Structures                     */
/* ============================================================= */

typedef struct ASTNode {
    NodeType ntype;
} ASTNode;

/* ---------- Function Definition ---------- */
typedef struct {
    NodeType ntype;
    char *name;
    int nargs;
    char **args;   /* Array of parameter names */
    void *body;    /* Pointer to body AST */
} FuncDefNode;

/* ---------- Function Call ---------- */
typedef struct {
    NodeType ntype;
    char *callee;
    void *args;    /* Expression list node */
} FuncCallNode;

/* ---------- Statement List ---------- */
typedef struct {
    NodeType ntype;
    void *head;
    void *rest;
} StmtListNode;

/* ---------- Expression List ---------- */
typedef struct {
    NodeType ntype;
    void *head;
    void *rest;
} ExprListNode;

/* ---------- Identifier ---------- */
typedef struct {
    NodeType ntype;
    char *name;
} IdentifierNode;

/* ---------- Integer Constant ---------- */
typedef struct {
    NodeType ntype;
    int value;
} IntConstNode;

/* ---------- Binary/Unary Expression ---------- */
typedef struct {
    NodeType ntype;
    void *operand1;
    void *operand2;   /* For unary ops, operand2 may be NULL */
} ExprNode;

/* ---------- If Statement ---------- */
typedef struct {
    NodeType ntype;
    void *expr;
    void *then_part;
    void *else_part;
} IfNode;

/* ---------- Assignment ---------- */
typedef struct {
    NodeType ntype;
    char *lhs;
    void *rhs;
} AssgNode;

/* ---------- While Statement ---------- */
typedef struct {
    NodeType ntype;
    void *expr;
    void *body;
} WhileNode;

/* ---------- Return Statement ---------- */
typedef struct {
    NodeType ntype;
    void *expr; /* NULL if return; has no expression */
} ReturnNode;


NodeType ast_node_type(void *ptr) {
    return ((ASTNode *)ptr)->ntype;
}

/* ---------- Function Definition ---------- */
char *func_def_name(void *ptr) {
    return ((FuncDefNode *)ptr)->name;
}

int func_def_nargs(void *ptr) {
    return ((FuncDefNode *)ptr)->nargs;
}

void *func_def_body(void *ptr) {
    return ((FuncDefNode *)ptr)->body;
}

char *func_def_argname(void *ptr, int n) {
    FuncDefNode *f = (FuncDefNode *)ptr;
    if (n <= 0 || n > f->nargs) return NULL;
    return f->args[n - 1];
}

/* ---------- Function Call ---------- */
char *func_call_callee(void *ptr) {
    return ((FuncCallNode *)ptr)->callee;
}

void *func_call_args(void *ptr) {
    return ((FuncCallNode *)ptr)->args;
}

/* ---------- Statement List ---------- */
void *stmt_list_head(void *ptr) {
    return ((StmtListNode *)ptr)->head;
}

void *stmt_list_rest(void *ptr) {
    return ((StmtListNode *)ptr)->rest;
}

/* ---------- Expression List ---------- */
void *expr_list_head(void *ptr) {
    return ((ExprListNode *)ptr)->head;
}

void *expr_list_rest(void *ptr) {
    return ((ExprListNode *)ptr)->rest;
}

/* ---------- Identifier ---------- */
char *expr_id_name(void *ptr) {
    return ((IdentifierNode *)ptr)->name;
}

/* ---------- Integer Constant ---------- */
int expr_intconst_val(void *ptr) {
    return ((IntConstNode *)ptr)->value;
}

/* ---------- Arithmetic / Boolean Expressions ---------- */
void *expr_operand_1(void *ptr) {
    return ((ExprNode *)ptr)->operand1;
}

void *expr_operand_2(void *ptr) {
    return ((ExprNode *)ptr)->operand2;
}

/* ---------- If Statement ---------- */
void *stmt_if_expr(void *ptr) {
    return ((IfNode *)ptr)->expr;
}

void *stmt_if_then(void *ptr) {
    return ((IfNode *)ptr)->then_part;
}

void *stmt_if_else(void *ptr) {
    return ((IfNode *)ptr)->else_part;
}

/* ---------- Assignment ---------- */
char *stmt_assg_lhs(void *ptr) {
    return ((AssgNode *)ptr)->lhs;
}

void *stmt_assg_rhs(void *ptr) {
    return ((AssgNode *)ptr)->rhs;
}

/* ---------- While Statement ---------- */
void *stmt_while_expr(void *ptr) {
    return ((WhileNode *)ptr)->expr;
}

void *stmt_while_body(void *ptr) {
    return ((WhileNode *)ptr)->body;
}

/* ---------- Return Statement ---------- */
void *stmt_return_expr(void *ptr) {
    return ((ReturnNode *)ptr)->expr;
}

/* ============================================================= */
/*              Optional Helpers for Node Construction            */
/* ============================================================= */
/* These aren't required by your spec but can be useful for your parser */

void *make_func_def(char *name, int nargs, char **args, void *body) {
    FuncDefNode *n = malloc(sizeof(FuncDefNode));
    n->ntype = FUNC_DEF;
    n->name = name;
    n->nargs = nargs;
    n->args = args;
    n->body = body;
    return n;
}

void *make_func_call(char *callee, void *args) {
    FuncCallNode *n = malloc(sizeof(FuncCallNode));
    n->ntype = FUNC_CALL;
    n->callee = callee;
    n->args = args;
    return n;
}

void *make_stmt_list(void *head, void *rest) {
    StmtListNode *n = malloc(sizeof(StmtListNode));
    n->ntype = STMT_LIST;
    n->head = head;
    n->rest = rest;
    return n;
}

void *make_expr_list(void *head, void *rest) {
    ExprListNode *n = malloc(sizeof(ExprListNode));
    n->ntype = EXPR_LIST;
    n->head = head;
    n->rest = rest;
    return n;
}

void *make_identifier(char *name) {
    IdentifierNode *n = malloc(sizeof(IdentifierNode));
    n->ntype = IDENTIFIER;
    n->name = name;
    return n;
}

void *make_intconst(int val) {
    IntConstNode *n = malloc(sizeof(IntConstNode));
    n->ntype = INTCONST;
    n->value = val;
    return n;
}

void *make_expr(NodeType op_type, void *lhs, void *rhs) {
    ExprNode *n = malloc(sizeof(ExprNode));
    n->ntype = op_type;
    n->operand1 = lhs;
    n->operand2 = rhs;
    return n;
}

void *make_if(void *expr, void *thenp, void *elsep) {
    IfNode *n = malloc(sizeof(IfNode));
    n->ntype = IF;
    n->expr = expr;
    n->then_part = thenp;
    n->else_part = elsep;
    return n;
}

void *make_assg(char *lhs, void *rhs) {
    AssgNode *n = malloc(sizeof(AssgNode));
    n->ntype = ASSG;
    n->lhs = lhs;
    n->rhs = rhs;
    return n;
}

void *make_while(void *expr, void *body) {
    WhileNode *n = malloc(sizeof(WhileNode));
    n->ntype = WHILE;
    n->expr = expr;
    n->body = body;
    return n;
}

void *make_return(void *expr) {
    ReturnNode *n = malloc(sizeof(ReturnNode));
    n->ntype = RETURN;
    n->expr = expr;
    return n;
}
