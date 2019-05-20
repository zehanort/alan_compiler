.PHONY: clean distclean default

CXX=g++
CXXFLAGS=-Wall -g -std=c++11 `llvm-config --cxxflags`
LDFLAGS=`llvm-config --ldflags --system-libs --libs all`

default: alan

lexer.cpp: lexer.l
	flex -s -o lexer.cpp lexer.l

lexer.o: lexer.cpp parser.hpp ast.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

parser.hpp parser.cpp: parser.ypp
	bison -dv -o parser.cpp parser.ypp

parser.o: parser.cpp parser.hpp ast.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

general.o  : general.cpp general.hpp error.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

error.o    : error.cpp error.hpp general.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

symbol.o   : symbol.cpp symbol.hpp general.hpp error.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

ast.o      : ast.cpp ast.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<

codegen.o  : codegen.cpp codegen.hpp
	$(CXX) $(CXXFLAGS) -ansi -pedantic -c $<	

# %.o: %.cpp
# 	$(CXX) $(CXXFLAGS) -c $<

alan: lexer.o parser.o ast.o codegen.o symbol.o error.o general.o
	$(CXX) $(CXXFLAGS) -o alan $^ $(LDFLAGS) /usr/lib/x86_64-linux-gnu/libfl.a

clean:
	$(RM) lexer.cpp parser.cpp parser.hpp parser.output *.o *~

distclean: clean
	$(RM) alan
