/*
 * File: codegen.h
 * Author: Hunter RUndhaug
 * Purpose: Entry points for final-code generation (AST -> TAC -> MIPS).
 * Author: Hunter Rundhaug
 *
 * Usage:
 *   - Call codegen_init_once() exactly once (or safely multiple times; it guards
 *     internally) before emitting any function code. It prints the runtime stub
 *     for println() to stdout.
 *   - For each function AST produced by the parser, call codegen_func(func_ast)
 *     to generate TAC and emit MIPS for that function to stdout.
 *
 * Requirements:
 *   - The AST node passed to codegen_func() must be a FUNC_DEF node created by
 *     your existing ast.c constructors and accessible via ast.h getters.
 */
#ifndef __CODEGEN_H__
#define __CODEGEN_H__


// Emit the required runtime for println() (prints once even if called multiple times). 
void codegen_init_once(void);

// Generate MIPS for a single function, given its FUNC_DEF AST node. */
void codegen_func(void *func_ast);


#endif /* __CODEGEN_H__ */