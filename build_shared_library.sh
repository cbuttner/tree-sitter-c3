#!/bin/sh
cc -shared -fPIC -fno-exceptions -O2 -static-libgcc -I src -xc src/parser.c src/scanner.c -o tree-sitter-c3.so
