# C-- Compiler in C

A lightweight compiler for a simplified C-like language, built from scratch in C.  
This project demonstrates core compiler construction concepts including lexical analysis, parsing, syntax validation, and translation toward lower-level output.

## Overview

This project was developed to explore the main phases of compilation by implementing a small compiler for the C-- language. The compiler processes source input, performs tokenization and parsing, and reports syntax-related issues with line-aware diagnostics.

## Features

- Lexical analysis using a custom scanner
- Parsing for C-- language constructs
- Error reporting with line-number tracking
- Driver programs for testing scanner and parser behavior
- Makefile-based build process

## Project Structure

- `scanner.c` / `scanner.h` — lexical analyzer implementation
- `parser.c` / `parser.h` — parser implementation
- `driver.c` — main compiler driver
- `scanner-driver.c` — standalone scanner testing
- `Makefile` — build automation
- `test.c`, `test2.c`, `exmpl.txt` — sample input files
- `out.txt`, `diff.txt` — output / comparison artifacts used during testing

## Build

To compile the project:

```bash
make
