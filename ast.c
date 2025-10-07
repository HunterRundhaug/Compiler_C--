#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

typedef struct ASTNode {
    NodeType type;

    // Common fields
    char *name;           // used for function names, identifiers, etc.
    int int_val;          // used for integer constants
    int nargs;            // used for function definitions

    // Substructure pointers
    struct ASTNode *op1;  // generic child 1 (operand1, expression, etc.)
    struct ASTNode *op2;  // generic child 2 (operand2, statement, etc.)
    struct ASTNode *next; // for stmt_list or expr_list linking

    // Function-specific
    struct ASTNode **formals;  // array of formal parameter name nodes
    struct ASTNode *body;      // function body (stmt list)

} ASTNode;
