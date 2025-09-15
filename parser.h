#ifndef __PARSER_H__
#define __PARSER_H__

#include "scanner.h"

// External declarations from scanner
extern int get_token(void);
extern char* lexeme;

// Parser function declaration
int parse(void);

#endif  /* __PARSER_H__ */
