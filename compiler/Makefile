.PHONY: clean distclean default

CC=gcc
CXX=g++
CFLAGS=-Wall -g
CXXFLAGS=-Wall -g -std=c++11

default: alan

lexer.cpp: lexer.l
	flex -s -o lexer.cpp lexer.l

lexer.o: lexer.cpp parser.hpp ast.hpp

parser.hpp parser.cpp: parser.y
	bison -dv -o parser.cpp parser.y

parser.o: parser.cpp parser.hpp ast.hpp

general.o  : general.c general.h error.h
	$(CC) $(CFLAGS) -ansi -pedantic -c $<

error.o    : error.c error.h general.h
	$(CC) $(CFLAGS) -ansi -pedantic -c $<

symbol.o   : symbol.c symbol.h general.h error.h
	$(CC) -g -ansi -pedantic -c $<

ast.o      : ast.cpp ast.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

alan: lexer.o parser.o ast.o general.o error.o symbol.o
	$(CC) $(CFLAGS) -o alan $^ -lfl

clean:
	$(RM) lexer.cpp parser.cpp parser.hpp parser.output *.o *~

distclean: clean
	$(RM) alan

