/*
 * File: codegen.c
 * Author: Hunter Rundhaug
 * Purpose: Final code generation (AST -> TAC -> MIPS) for Milestone 1 (G2 w/o control flow/return).
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
    TAC_LEAVE,     /* leave f          : x = func name */

    TAC_COPY,      /* x := y           : x=dst, y=src (src may be #imm) */

    TAC_PARAM,     /* param x          : x may be #imm */
    TAC_CALL       /* call p, n        : x = callee name, n = argc   */
} TacKind;

typedef struct Tac {
    TacKind op;
    char   *x;     
    char   *y;    
    int     n;     
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

static Tac *tac_enter(const char *fname) { Tac *t=tac_mk0(TAC_ENTER); t->x=strdup(fname); return t; }
static Tac *tac_leave(const char *fname) { Tac *t=tac_mk0(TAC_LEAVE); t->x=strdup(fname); return t; }

static Tac *tac_copy (const char *dst, const char *src) { Tac *t=tac_mk0(TAC_COPY); t->x=strdup(dst); t->y=strdup(src); return t; }
static Tac *tac_param(const char *a)   { Tac *t=tac_mk0(TAC_PARAM); t->x=strdup(a); return t; }
static Tac *tac_call (const char *p,int n){ Tac *t=tac_mk0(TAC_CALL); t->x=strdup(p); t->n=n; return t; }

static int is_imm(const char *s) { return s && s[0] == '#'; }
static int imm_val(const char *s) { return atoi(s ? s+1 : "0"); }

/* ------------------------
 *   Local stack allocation
 * ------------------------ */

typedef struct {
    char *name;
    int   offset;   /* negative offset from $fp */
} Local;

typedef struct {
    Local *arr;
    int    len;
    int    cap;
    int    next_off; /* next negative offset (starts at -4, then -8, ...) */
} Locals;

static void locals_init(Locals *L) {
    L->arr = NULL; L->len = 0; L->cap = 0; L->next_off = -4;
}
static void locals_free(Locals *L) {
    for (int i=0;i<L->len;i++) free(L->arr[i].name);
    free(L->arr);
    locals_init(L);
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
 *  PARAM/LOCAL/GLOBAL classify
 * ------------------------ */

typedef enum { CL_PARAM, CL_LOCAL, CL_GLOBAL } Class;

static int formal_index_of(void *func_ast, const char *name){
    int n = func_def_nargs(func_ast);
    for (int i=1;i<=n;i++){
        const char *arg = func_def_argname(func_ast, i);
        if (arg && strcmp(arg, name)==0) return i; /* 1-based */
    }
    return 0;
}

// Use current function scope (still open during codegen) + globals 
static Class classify_ident(void *func_ast, const char *name){
    if (!name || !*name) return CL_LOCAL;
    if (formal_index_of(func_ast, name) > 0) return CL_PARAM;

    // local?  lookup_in_current_scope returns 0 when found in your parser 
    int loc = lookup_in_current_scope((char*)name, SYM_INT_VAR);
    if (loc == 0) return CL_LOCAL;

    /* global? lookup_global_scope returned 0 when found in your parser */
    int glob = lookup_global_scope((char*)name, SYM_INT_VAR);
    if (glob == 0) return CL_GLOBAL;

    /* default: treat as local */
    return CL_LOCAL;
}

/* Compute stack offset for param i (1-based) with our prologue layout */
static int param_offset_1based(int i){
    return 8 + 4*(i-1);
}

/* ------------------------
 *   AST -> TAC (Milestone 1)
 * ------------------------ */

static void ast_to_tac_stmt(void *stmt);   /* fwd */

static void ast_to_tac_stmt_list(void *sl) {
    while (sl) {
        void *head = stmt_list_head(sl);
        if (head) ast_to_tac_stmt(head);
        sl = stmt_list_rest(sl);
    }
}

/* Convert expr (ID | INTCONST for Milestone 1) to a TAC operand string. */
static char *expr_as_operand(void *expr) {
    NodeType t = ast_node_type(expr);
    if (t == IDENTIFIER) {
        const char *nm = expr_id_name(expr);
        return strdup(nm ? nm : "");
    } else if (t == INTCONST) {
        int v = expr_intconst_val(expr);
        char buf[64]; snprintf(buf, sizeof(buf), "#%d", v);
        return strdup(buf);
    } else {
        return strdup("#0");
    }
}

static void ast_to_tac_call_generic(void *call_node){
    const char *callee = func_call_callee(call_node);
    void *args = func_call_args(call_node);

    /* Collect args left-to-right, then emit PARAM in right-to-left order */
    int n = 0;
    void *p = args;
    while (p){ n++; p = expr_list_rest(p); }
    char **ops = (char**)malloc(sizeof(char*)*n);
    p = args;
    for(int i=0;i<n;i++){
        void *e = expr_list_head(p);
        ops[i] = expr_as_operand(e);
        p = expr_list_rest(p);
    }
    for (int i=n-1; i>=0; --i) tac_emit(tac_param(ops[i]));
    tac_emit(tac_call(callee ? callee : "", n));
    for (int i=0;i<n;i++) free(ops[i]);
    free(ops);
}

static void ast_to_tac_stmt(void *stmt) {
    if (!stmt) return;
    NodeType k = ast_node_type(stmt);

    switch (k) {
        case ASSG: {
            const char *lhs = stmt_assg_lhs(stmt);
            void *rhs = stmt_assg_rhs(stmt);
            char *src = expr_as_operand(rhs);
            tac_emit(tac_copy(lhs, src));
            free(src);
            break;
        }
        case FUNC_CALL: {
            /* Handle any call uniformly (println or user functions) */
            ast_to_tac_call_generic(stmt);
            break;
        }
        case STMT_LIST: {
            ast_to_tac_stmt_list(stmt);
            break;
        }
        /* Milestone 1: ignore IF/WHILE/RETURN */
        default:
            break;
    }
}

static void ast_func_to_tac(void *func_ast) {
    
    const char *fname = func_def_name(func_ast);
    void *body = func_def_body(func_ast);
    tac_emit(tac_enter(fname ? fname : "fn"));
    if (body) ast_to_tac_stmt_list(body);
    tac_emit(tac_leave(fname ? fname : "fn"));
}

/* ------------------------
 *     TAC to MIPS lowering
 * ------------------------ */

static void emit_runtime_println_once(void);
static void emit_prologue(const char *fname, int frame_size);
static void emit_epilogue(const char *fname);

/* Emit MIPS for TAC list with a simple frame & locals map. */
static void tac_to_mips(void *func_ast) {
    const char *fname = func_def_name(func_ast);
    if (!fname) fname = "fn";

    Locals L; locals_init(&L);

    /* First pass: assign local slots (locals only; params/globals not placed here). */
    for (Tac *t = tac_head; t; t = t->next) {
        if (t->op == TAC_COPY) {
            if (t->x && !is_imm(t->x) && classify_ident(func_ast, t->x)==CL_LOCAL)
                (void)locals_offset_of(&L, t->x);
            if (t->y && !is_imm(t->y) && classify_ident(func_ast, t->y)==CL_LOCAL)
                (void)locals_offset_of(&L, t->y);
        } else if (t->op == TAC_PARAM) {
            if (t->x && !is_imm(t->x) && classify_ident(func_ast, t->x)==CL_LOCAL)
                (void)locals_offset_of(&L, t->x);
        }
    }

    /* Prologue */
    int frame = locals_frame_size(&L);
    emit_prologue(fname, frame);


    /* Body */
    for (Tac *t = tac_head; t; t = t->next) {
        switch (t->op) {
            case TAC_COPY: {
                /* x := y */
                /* Load src into $t0 */
                if (is_imm(t->y)) {
                    int v = imm_val(t->y);
                    printf("  li   $t0, %d\n", v);
                } else {
                    Class csrc = classify_ident(func_ast, t->y);
                    if (csrc == CL_PARAM) {
                        int idx = formal_index_of(func_ast, t->y);
                        int off = param_offset_1based(idx);
                        printf("  lw   $t0, %d($fp)\n", off);
                    } else if (csrc == CL_GLOBAL) {
                        ensure_global_emitted(t->y);
                        printf("  lw   $t0, %s\n", t->y);
                    } else { /* local */
                        int off = locals_offset_of(&L, t->y);
                        printf("  lw   $t0, %d($fp)\n", off);
                    }
                }
                /* Store to dst */
                if (!is_imm(t->x)) {
                    Class cdst = classify_ident(func_ast, t->x);
                    if (cdst == CL_PARAM) {
                        int idx = formal_index_of(func_ast, t->x);
                        int off = param_offset_1based(idx);
                        printf("  sw   $t0, %d($fp)\n", off);
                    } else if (cdst == CL_GLOBAL) {
                        ensure_global_emitted(t->x);
                        printf("  sw   $t0, %s\n", t->x);
                    } else { /* local */
                        int off = locals_offset_of(&L, t->x);
                        printf("  sw   $t0, %d($fp)\n", off);
                    }
                }
                break;
            }
            case TAC_PARAM: {
                /* push actual in right-to-left order */
                if (is_imm(t->x)) {
                    int v = imm_val(t->x);
                    printf("  li   $t0, %d\n", v);
                } else {
                    Class c = classify_ident(func_ast, t->x);
                    if (c == CL_PARAM) {
                        int idx = formal_index_of(func_ast, t->x);
                        int off = param_offset_1based(idx);
                        printf("  lw   $t0, %d($fp)\n", off);
                    } else if (c == CL_GLOBAL) {
                        ensure_global_emitted(t->x);
                        printf("  lw   $t0, %s\n", t->x);
                    } else { /* local */
                        int off = locals_offset_of(&L, t->x);
                        printf("  lw   $t0, %d($fp)\n", off);
                    }
                }
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

    /* We don't know globals upfront, we emit them when first used.
       But we must still emit the println runtime. */
    emit_runtime_println_once();
}


void codegen_func(void *func_ast) {
    if (!func_ast || ast_node_type(func_ast) != FUNC_DEF) return;

    /* AST to TAC (Milestone 1 subset) */
    tac_reset();
    ast_func_to_tac(func_ast);

    /* TAC to MIPS with symtab-based storage classification */
    tac_to_mips(func_ast);

    /* free TAC list */
    Tac *t = tac_head;
    while (t) {
        Tac *n = t->next;
        free(t->x);
        free(t->y);
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
