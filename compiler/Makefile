.PHONY: clean distclean default

CC=gcc
CFLAGS=-Wall -g

default: alan

lexer.c: lexer.l
	flex -s -o lexer.c lexer.l

lexer.o: lexer.c parser.h

parser.h parser.c: parser.y
	bison -dv -o parser.c parser.y

parser.o: parser.c

general.o  : general.c general.h error.h
	$(CC) $(CFLAGS) -ansi -pedantic -c $<

error.o    : error.c general.h error.h
	$(CC) $(CFLAGS) -ansi -pedantic -c $<

symbol.o   : symbol.c symbol.h general.h error.h
	$(CC) -g -ansi -pedantic -c $<

alan: lexer.o parser.o ast.o general.o error.o symbol.o
	$(CC) $(CFLAGS) -o alan $^ -lfl

clean:
	$(RM) lexer.c parser.c parser.h parser.output *.o *~

distclean: clean
	$(RM) alan

