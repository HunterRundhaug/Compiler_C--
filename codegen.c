/*
 * File: codegen.c
 * Author: Hunter Rundhaug
 * Purpose: Final code generation (AST -> TAC -> MIPS) for Milestone 2 (G2 incl. control flow/return).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "codegen.h"
#include "symtab.h"

/* ------------------------
 *   Minimal TAC (3-address)
 * ------------------------*/

typedef enum {
    TAC_ENTER,     /* enter f          : x = func name */
    TAC_LEAVE,     /* leave f            : x = func name */

    TAC_COPY,      /* x := y            : x=dst, y=src (src may be #imm) */
    TAC_ADD,       /* x := y + z         */
    TAC_SUB,       /* x := y - z         */
    TAC_MUL,       /* x := y * z         */
    TAC_DIV,       /* x := y / z         */

    TAC_PARAM,     /* param x            : x may be #imm */
    TAC_CALL,      /* call p, n         : x = callee name, n = argc */

    TAC_LABEL,     /* label L            : x = label name */
    TAC_GOTO,      /* goto L             : x = label name */

    /* conditional branches (relational operators) */
    TAC_IFEQ,      /* if (y == z) goto x */
    TAC_IFNE,
    TAC_IFLT,
    TAC_IFLE,
    TAC_IFGT,
    TAC_IFGE,

    TAC_RETURN     /* return (optional y) */
} TacKind;

typedef struct Tac {
    TacKind op;
    char   *x;     /* dst or label target (or callee name) */
    char   *y;     /* src1 */
    char   *z;     /* src2 (for binops / if relops) */
    int     n;     /* argc for call */
    struct Tac *next;
} Tac;

static Tac *tac_head = NULL, *tac_tail = NULL;

static void tac_reset(void) { tac_head = tac_tail = NULL; }

static void tac_emit(Tac *t) {
    if (!tac_head) tac_head = tac_tail = t;
    else tac_tail = tac_tail->next = t;
}

static Tac *tac_mk0(TacKind k) {
    Tac *t = (Tac*)calloc(1, sizeof(*t));
    t->op = k;
    return t;
}
static Tac *tac_mk1(TacKind k, const char *x) {
    Tac *t = tac_mk0(k);
    t->x = x ? strdup(x) : NULL;
    return t;
}
static Tac *tac_mk2(TacKind k, const char *x, const char *y) {
    Tac *t = tac_mk0(k);
    t->x = x ? strdup(x) : NULL;
    t->y = y ? strdup(y) : NULL;
    return t;
}
static Tac *tac_mk3(TacKind k, const char *x, const char *y, const char *z) {
    Tac *t = tac_mk0(k);
    t->x = x ? strdup(x) : NULL;
    t->y = y ? strdup(y) : NULL;
    t->z = z ? strdup(z) : NULL;
    return t;
}

static Tac *tac_enter(const char *fname)     { return tac_mk1(TAC_ENTER, fname); }
static Tac *tac_leave(const char *fname)     { return tac_mk1(TAC_LEAVE, fname); }
static Tac *tac_copy (const char *dst, const char *src) { return tac_mk2(TAC_COPY, dst, src); }
static Tac *tac_binop(TacKind k,const char *dst,const char *a,const char *b){ return tac_mk3(k,dst,a,b); }
static Tac *tac_param(const char *a)         { return tac_mk1(TAC_PARAM, a); }
static Tac *tac_call (const char *p,int n)   { Tac *t=tac_mk1(TAC_CALL, p); t->n=n; return t; }
static Tac *tac_label(const char *L)         { return tac_mk1(TAC_LABEL, L); }
static Tac *tac_goto (const char *L)         { return tac_mk1(TAC_GOTO,  L); }
static Tac *tac_if(TacKind k,const char *L,const char *a,const char *b){ return tac_mk3(k,L,a,b); }
static Tac *tac_return0(void)                { return tac_mk0(TAC_RETURN); }
static Tac *tac_return1(const char *a)       { return tac_mk2(TAC_RETURN, NULL, a); }

static int is_imm(const char *s) { return s && s[0] == '#'; }
static int imm_val(const char *s) { return atoi(s ? s+1 : "0"); }

/* ------------------------
 *   Name factories (temps/labels)
 * ------------------------ */

static int temp_no = 0;
static int label_no = 0;

static char *new_temp_name(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "_t%d", temp_no++);
    return strdup(buf);
}
static char *new_label_name(void) {
    char buf[64];
    snprintf(buf, sizeof(buf), "L%d", label_no++);
    return strdup(buf);
}

/* ------------------------
 *   Local stack allocation
 * ----------- ------- ----- */

typedef struct {
    char *name;
    int   offset;   /* negative offset from $fp */
} Local;

typedef struct {
    Local *arr;
    int    len;
    int    cap;
    int    next_off; /* next negative offset (starts at -4, then -8, */
} Locals;

static void locals_init(Locals *L) {
    L->arr = NULL; L->len = 0; L->cap = 0; L->next_off = -4;
}
static void locals_free(Locals *L) {
    for (int i=0;i<L->len;i++) free(L->arr[i].name);
    free(L->arr);
    L->arr=NULL; L->len=L->cap=0; L->next_off=-4;
}
static void locals_reserve(Locals *L, int need) {
    if (L->cap >= need) return;
    int ncap = L->cap ? L->cap*2 : 8;
    if (ncap < need) ncap = need;
    L->arr = (Local*)realloc(L->arr, ncap*sizeof(Local));
    L->cap = ncap;
}
static int locals_find(Locals *L, const char *name) {
    for (int i=0;i<L->len;i++) if (strcmp(L->arr[i].name, name)==0) return i;
    return -1;
}
static int locals_offset_of(Locals *L, const char *name) {
    if (!name) return 0;
    int i = locals_find(L, name);
    if (i >= 0) return L->arr[i].offset;
    /* new local */
    locals_reserve(L, L->len+1);
    L->arr[L->len].name   = strdup(name);
    L->arr[L->len].offset = L->next_off;
    L->len += 1;
    L->next_off -= 4;
    return L->arr[L->len-1].offset;
}
static int locals_frame_size(const Locals *L) {
    return L->len * 4;
}

/* ------------------------
 *   Global data tracking
 * ------------------------ */

typedef struct {
    char **names;
    int    len, cap;
} GSet;

static GSet gset = { NULL, 0, 0 };

static int gset_has(const char *name){
    for(int i=0;i<gset.len;i++) if(strcmp(gset.names[i],name)==0) return 1;
    return 0;
}
static void gset_add(const char *name){
    if (gset_has(name)) return;
    if (gset.len == gset.cap) {
        gset.cap = gset.cap ? 2*gset.cap : 8;
        gset.names = (char**)realloc(gset.names, gset.cap*sizeof(char*));
    }
    gset.names[gset.len++] = strdup(name);
}

// Emit a .data word for a global variable once, then go switch back to .text
static void ensure_global_emitted(const char *name){
    if (gset_has(name)) return;
    gset_add(name);
    puts(".data");
    printf("%s: .word 0\n", name);
    puts(".text");
}

/* ------------------------
 *  PARAM LOCAL GLOBAL classify
 * ------------------------ */

typedef enum { CL_PARAM, CL_LOCAL, CL_GLOBAL } Class;

static int formal_index_of(void *func_ast, const char *name){
    int n = func_def_nargs(func_ast);
    for (int i=1;i<=n;i++){
        char *arg = func_def_argname(func_ast, i);
        if (arg && strcmp(arg, name)==0) return i; /* 1-based */
    }
    return 0;
}

// Use current function scope (still open during codegen) + globals
static Class classify_ident(void *func_ast, const char *name){
    if (!name || !*name) return CL_LOCAL;
    if (formal_index_of(func_ast, name) > 0) return CL_PARAM;

    /* local?  lookup_in_current_scope returns 0 when found in parser */
    if (lookup_in_current_scope((char*)name, SYM_INT_VAR) == 0) return CL_LOCAL;

    /* global? lookup_global_scope returned 0 when found */
    if (lookup_global_scope((char*)name, SYM_INT_VAR) == 0) return CL_GLOBAL;

    /* temporaries or unknowns: treat as local */
    return CL_LOCAL;
}

/* Compute stack offset for param i (1-based) with our prologue layout */
static int param_offset_1based(int i){
    return 8 + 4*(i-1);
}

/* ------------------------
 *   AST -> TAC helpers
 * ------------------------ */

static char *gen_expr_val(void *func_ast, void *expr);     /* returns operand string (temp, id, or #imm) */
static void ast_to_tac_stmt(void *func_ast, void *stmt);   /* forward */

static void ast_to_tac_stmt_list(void *func_ast, void *sl) {
    while (sl) {
        void *head = stmt_list_head(sl);
        if (head) ast_to_tac_stmt(func_ast, head);
        sl = stmt_list_rest(sl);
    }
}

/* Convert a primary to an operand string (id, int, or generated temp). */
static char *expr_as_operand_prim(void *expr) {
    NodeType t = ast_node_type(expr);
    if (t == IDENTIFIER) {
        char *nm = expr_id_name(expr);
        return strdup(nm ? nm : "");
    } else if (t == INTCONST) {
        int v = expr_intconst_val(expr);
        char buf[64]; snprintf(buf, sizeof(buf), "#%d", v);
        return strdup(buf);
    }
    return NULL;
}

/* Generate TAC (and return a place string) for arithmetic expr (incl. unary minus, binops). */
static char *gen_expr_val(void *func_ast, void *expr) {
    if (!expr) return strdup("#0");
    NodeType t = ast_node_type(expr);

    /* identifiers & int constants */
    if (t == IDENTIFIER || t == INTCONST) {
        char *p = expr_as_operand_prim(expr);
        if (p) return p;
    }

    /* function call used as expression (optional support) */
    if (t == FUNC_CALL) {
        char *callee = func_call_callee(expr);
        void *args = func_call_args(expr);

        /* collect args L->R, emit PARAM in R->L */
        int n = 0; void *p = args;
        while (p){ n++; p = expr_list_rest(p); }
        char **ops = (char**)malloc(sizeof(char*)*n);
        p = args;
        for(int i=0;i<n;i++){
            void *e = expr_list_head(p);
            ops[i] = gen_expr_val(func_ast, e);
            p = expr_list_rest(p);
        }
        for (int i=n-1; i>=0; --i) { tac_emit(tac_param(ops[i])); free(ops[i]); }
        free(ops);

        tac_emit(tac_call(callee ? callee : "", n));
        /* no return value convention; return #0 so caller can proceed */
        return strdup("#0");
    }

    /* unary minus encoded as NodeType UMINUS */
    if (t == UMINUS) {
        void *E = expr_operand_1(expr);
        char *a = gen_expr_val(func_ast, E);
        char *dst = new_temp_name();
        tac_emit(tac_binop(TAC_SUB, dst, "#0", a));
        free(a);
        return dst;
    }

    /* binary arithmetic ops */
    if (t == ADD || t == SUB || t == MUL || t == DIV) {
        void *L = expr_operand_1(expr);
        void *R = expr_operand_2(expr);
        char *a = gen_expr_val(func_ast, L);
        char *b = gen_expr_val(func_ast, R);
        char *dst = new_temp_name();
        TacKind k = (t==ADD)?TAC_ADD : (t==SUB)?TAC_SUB : (t==MUL)?TAC_MUL : TAC_DIV;
        tac_emit(tac_binop(k, dst, a, b));
        free(a); free(b);
        return dst;
    }

    /* relational/logical are handled by control flow, not as arithmetic values */
    return strdup("#0");
}

/* Emit PARAMs + CALL for a call used as a statement (println or others) */
static void ast_to_tac_call_stmt(void *func_ast, void *call_node){
    char *callee = func_call_callee(call_node);
    void *args = func_call_args(call_node);

    int n = 0; void *p = args;
    while (p){ n++; p = expr_list_rest(p); }
    char **ops = (char**)malloc(sizeof(char*)*n);
    p = args;
    for(int i=0;i<n;i++){
        void *e = expr_list_head(p);
        ops[i] = gen_expr_val(func_ast, e);
        p = expr_list_rest(p);
    }
    for (int i=n-1; i>=0; --i) { tac_emit(tac_param(ops[i])); free(ops[i]); }
    free(ops);

    tac_emit(tac_call(callee ? callee : "", n));
}

/* Generate TAC for a boolean test: emit one IFxx to Ltrue then GOTO Lfalse. */
static void gen_bool_jump(void *func_ast, void *boolexpr, const char *Ltrue, const char *Lfalse) {
    NodeType t = ast_node_type(boolexpr);
    if (t==EQ||t==NE||t==LT||t==LE||t==GT||t==GE) {
        char *a = gen_expr_val(func_ast, expr_operand_1(boolexpr));
        char *b = gen_expr_val(func_ast, expr_operand_2(boolexpr));
        TacKind cond =
          (t==EQ)?TAC_IFEQ : (t==NE)?TAC_IFNE : (t==LT)?TAC_IFLT :
          (t==LE)?TAC_IFLE : (t==GT)?TAC_IFGT : TAC_IFGE;
        tac_emit(tac_if(cond, Ltrue, a, b));
        tac_emit(tac_goto(Lfalse));
        free(a); free(b);
        return;
    }

    /* If it's not a relational, evaluate expression != 0 */
    char *v = gen_expr_val(func_ast, boolexpr);
    tac_emit(tac_if(TAC_IFNE, Ltrue, v, "#0"));
    tac_emit(tac_goto(Lfalse));
    free(v);
}

/* Convert statements */
static void ast_to_tac_stmt(void *func_ast, void *stmt) {
    if (!stmt) return;
    NodeType k = ast_node_type(stmt);

    switch (k) {
        case ASSG: {
            char *lhs = stmt_assg_lhs(stmt);
            char *rhsv = gen_expr_val(func_ast, stmt_assg_rhs(stmt));
            tac_emit(tac_copy(lhs, rhsv));
            free(rhsv);
            break;
        }
        case FUNC_CALL: {
            ast_to_tac_call_stmt(func_ast, stmt);
            break;
        }
        case IF: {
            void *cond = stmt_if_expr(stmt);
            void *thenp = stmt_if_then(stmt);
            void *elsep = stmt_if_else(stmt);

            char *Lthen  = new_label_name();
            char *Lelse  = new_label_name();
            char *Lafter = new_label_name();

            gen_bool_jump(func_ast, cond, Lthen, Lelse);
            tac_emit(tac_label(Lthen));
            ast_to_tac_stmt(func_ast, thenp);
            tac_emit(tac_goto(Lafter));
            tac_emit(tac_label(Lelse));
            if (elsep) ast_to_tac_stmt(func_ast, elsep);
            tac_emit(tac_label(Lafter));

            free(Lthen); free(Lelse); free(Lafter);
            break;
        }
        case WHILE: {
            void *cond = stmt_while_expr(stmt);
            void *body = stmt_while_body(stmt);

            char *Ltop   = new_label_name();
            char *Lbody  = new_label_name();
            char *Lafter = new_label_name();

            tac_emit(tac_label(Ltop));
            gen_bool_jump(func_ast, cond, Lbody, Lafter);
            tac_emit(tac_label(Lbody));
            ast_to_tac_stmt(func_ast, body);
            tac_emit(tac_goto(Ltop));
            tac_emit(tac_label(Lafter));

            free(Ltop); free(Lbody); free(Lafter);
            break;
        }
        case RETURN: {
            void *e = stmt_return_expr(stmt);
            if (e) {
                char *v = gen_expr_val(func_ast, e);
                tac_emit(tac_return1(v));
                free(v);
            } else {
                tac_emit(tac_return0());
            }
            break;
        }
        case STMT_LIST: {
            ast_to_tac_stmt_list(func_ast, stmt);
            break;
        }
        default:
            /* ignore unexpected kinds */
            break;
    }
}

static void ast_func_to_tac(void *func_ast) {
    char *fname = func_def_name(func_ast);
    void *body = func_def_body(func_ast);
    tac_emit(tac_enter(fname ? fname : "fn"));
    if (body) ast_to_tac_stmt_list(func_ast, body);
    tac_emit(tac_leave(fname ? fname : "fn"));
}

/* ------------------------
 *     MIPS helpers
 * ------------------------ */

static void emit_runtime_println_once(void);
static void emit_prologue(const char *fname, int frame_size);
static void emit_epilogue(const char *fname);
static void mips_load_into(const char *reg, void *func_ast, Locals *L, const char *opnd);
static void mips_store_from(const char *reg, void *func_ast, Locals *L, const char *dst);
static void mips_binop3(const char *mipsop, void *func_ast, Locals *L, const char *dst, const char *a, const char *b);
static void mips_branch_rel(TacKind rel, void *func_ast, Locals *L, const char *Ldst, const char *a, const char *b);

/* ------------------------
 *     TAC to MIPS lowering
 * ------------------------ */

static void tac_to_mips(void *func_ast) {
    char *fname = func_def_name(func_ast);
    if (!fname) fname = "fn";

    Locals L; locals_init(&L);

    /* First pass: assign local slots (locals + temps; params/globals not in locals) */
    for (Tac *t = tac_head; t; t = t->next) {
        switch (t->op) {
            case TAC_COPY:
                if (t->x && !is_imm(t->x) && classify_ident(func_ast, t->x)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->x);
                if (t->y && !is_imm(t->y) && classify_ident(func_ast, t->y)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->y);
                break;
            case TAC_ADD: case TAC_SUB: case TAC_MUL: case TAC_DIV:
                if (t->x && !is_imm(t->x) && classify_ident(func_ast, t->x)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->x);
                if (t->y && !is_imm(t->y) && classify_ident(func_ast, t->y)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->y);
                if (t->z && !is_imm(t->z) && classify_ident(func_ast, t->z)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->z);
                break;
            case TAC_PARAM:
                if (t->x && !is_imm(t->x) && classify_ident(func_ast, t->x)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->x);
                break;
            case TAC_IFEQ: case TAC_IFNE: case TAC_IFLT: case TAC_IFLE:
            case TAC_IFGT: case TAC_IFGE:
                if (t->y && !is_imm(t->y) && classify_ident(func_ast, t->y)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->y);
                if (t->z && !is_imm(t->z) && classify_ident(func_ast, t->z)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->z);
                break;
            case TAC_RETURN:
                if (t->y && !is_imm(t->y) && classify_ident(func_ast, t->y)==CL_LOCAL)
                    (void)locals_offset_of(&L, t->y);
                break;
            default: break;
        }
    }

    /* Prologue */
    int frame = locals_frame_size(&L);
    emit_prologue(fname, frame);

    /* Body */
    for (Tac *t = tac_head; t; t = t->next) {
        switch (t->op) {
            case TAC_COPY: {
                mips_load_into("$t0", func_ast, &L, t->y);
                mips_store_from("$t0", func_ast, &L, t->x);
                break;
            }
            case TAC_ADD: mips_binop3("add", func_ast, &L, t->x, t->y, t->z); break;
            case TAC_SUB: mips_binop3("sub", func_ast, &L, t->x, t->y, t->z); break;
            case TAC_MUL: mips_binop3("mul", func_ast, &L, t->x, t->y, t->z); break;
            case TAC_DIV: {
                mips_load_into("$t0", func_ast, &L, t->y);
                mips_load_into("$t1", func_ast, &L, t->z);
                printf("  div  $t0, $t1\n");
                printf("  mflo $t2\n");
                mips_store_from("$t2", func_ast, &L, t->x);
                break;
            }

            case TAC_PARAM: {
                mips_load_into("$t0", func_ast, &L, t->x);
                printf("  addiu $sp, $sp, -4\n");
                printf("  sw   $t0, 0($sp)\n");
                break;
            }
            case TAC_CALL: {
                if (t->x && strcmp(t->x, "println")==0) {
                    printf("  jal  _println\n");
                } else {
                    printf("  jal  %s\n", t->x && *t->x ? t->x : "fn");
                }
                if (t->n > 0) printf("  addiu $sp, $sp, %d\n", 4 * t->n);
                break;
            }

            case TAC_LABEL: printf("%s:\n", t->x ? t->x : "L"); break;
            case TAC_GOTO:  printf("  j    %s\n", t->x ? t->x : "L"); break;

            case TAC_IFEQ: case TAC_IFNE: case TAC_IFLT:
            case TAC_IFLE: case TAC_IFGT: case TAC_IFGE:
                mips_branch_rel(t->op, func_ast, &L, t->x, t->y, t->z);
                break;

            case TAC_RETURN: {
                if (t->y) {
                    mips_load_into("$v0", func_ast, &L, t->y); /* return value in $v0 */
                }
                printf("  j    Lret_%s\n", fname);
                break;
            }

            case TAC_ENTER:
            case TAC_LEAVE:
                /* handled by prologue/epilogue emitters */
                break;
        }
    }

    /* Epilogue */
    emit_epilogue(fname);

    locals_free(&L);
}

/* ------------------------
 *      Public entry points
 * ------------------------ */

void codegen_init_once(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    emit_runtime_println_once();
}

void codegen_func(void *func_ast) {
    if (!func_ast || ast_node_type(func_ast) != FUNC_DEF) return;

    /* reset temps per function */
    temp_no = 0;

    /* AST -> TAC */
    tac_reset();
    ast_func_to_tac(func_ast);

    /* TAC -> MIPS */
    tac_to_mips(func_ast);

    /* free TAC list */
    Tac *t = tac_head;
    while (t) {
        Tac *n = t->next;
        free(t->x);
        free(t->y);
        free(t->z);
        free(t);
        t = n;
    }
    tac_head = tac_tail = NULL;
}

/* ------------------------
 *        MIPS Emitters
 * ------------------------ */

static void emit_runtime_println_once(void) {
    puts(".align 2");
    puts(".data");
    puts("_nl: .asciiz \"\\n\"");
    puts(".align 2");
    puts(".text");
    puts("# println: print out an integer followed by a newline");
    puts("_println:");
    puts("  li   $v0, 1");
    puts("  lw   $a0, 0($sp)");
    puts("  syscall");
    puts("  li   $v0, 4");
    puts("  la   $a0, _nl");
    puts("  syscall");
    puts("  jr   $ra");
    puts("");
}

static void emit_prologue(const char *fname, int frame_size) {
    printf("%s:\n", fname);
    printf("  addiu $sp, $sp, -8\n");
    printf("  sw   $ra, 4($sp)\n");
    printf("  sw   $fp, 0($sp)\n");
    printf("  move $fp, $sp\n");
    if (frame_size > 0) {
        printf("  addiu $sp, $sp, -%d\n", frame_size);
    }
}

static void emit_epilogue(const char *fname) {
    printf("Lret_%s:\n", fname);
    printf("  move $sp, $fp\n");
    printf("  lw   $fp, 0($sp)\n");
    printf("  lw   $ra, 4($sp)\n");
    printf("  addiu $sp, $sp, 8\n");
    printf("  jr   $ra\n\n");
}

/* ------------------------
 *        MIPS helpers
 * ------------------------ */

static void mips_load_into(const char *reg, void *func_ast, Locals *L, const char *opnd) {
    if (is_imm(opnd)) {
        printf("  li   %s, %d\n", reg, imm_val(opnd));
        return;
    }
    Class c = classify_ident(func_ast, opnd);
    if (c == CL_PARAM) {
        int idx = formal_index_of(func_ast, opnd);
        int off = param_offset_1based(idx);
        printf("  lw   %s, %d($fp)\n", reg, off);
    } else if (c == CL_GLOBAL) {
        ensure_global_emitted(opnd);
        printf("  lw   %s, %s\n", reg, opnd);
    } else { /* local or temp */
        int off = locals_offset_of(L, opnd);
        printf("  lw   %s, %d($fp)\n", reg, off);
    }
}

static void mips_store_from(const char *reg, void *func_ast, Locals *L, const char *dst) {
    if (is_imm(dst)) return; /* shouldn't happen for dests, but guard anyway */
    Class c = classify_ident(func_ast, dst);
    if (c == CL_PARAM) {
        int idx = formal_index_of(func_ast, dst);
        int off = param_offset_1based(idx);
        printf("  sw   %s, %d($fp)\n", reg, off);
    } else if (c == CL_GLOBAL) {
        ensure_global_emitted(dst);
        printf("  sw   %s, %s\n", reg, dst);
    } else { /* local or temp */
        int off = locals_offset_of(L, dst);
        printf("  sw   %s, %d($fp)\n", reg, off);
    }
}

static void mips_binop3(const char *mipsop, void *func_ast, Locals *L, const char *dst, const char *a, const char *b) {
    mips_load_into("$t0", func_ast, L, a);
    mips_load_into("$t1", func_ast, L, b);
    printf("  %s  $t2, $t0, $t1\n", mipsop);
    mips_store_from("$t2", func_ast, L, dst);
}

static void mips_branch_rel(TacKind rel, void *func_ast, Locals *L, const char *Ldst, const char *a, const char *b) {
    mips_load_into("$t0", func_ast, L, a);
    mips_load_into("$t1", func_ast, L, b);
    switch (rel) {
        case TAC_IFEQ: printf("  beq  $t0, $t1, %s\n", Ldst); break;
        case TAC_IFNE: printf("  bne  $t0, $t1, %s\n", Ldst); break;
        case TAC_IFLT: printf("  blt  $t0, $t1, %s\n", Ldst); break;
        case TAC_IFLE: printf("  ble  $t0, $t1, %s\n", Ldst); break;
        case TAC_IFGT: printf("  bgt  $t0, $t1, %s\n", Ldst); break;
        case TAC_IFGE: printf("  bge  $t0, $t1, %s\n", Ldst); break;
        default: break;
    }
}
