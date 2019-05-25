.PHONY: clean distclean default

SRCDIR=src
INCDIR=include
LIBDIR=lib
BUILDDIR=build
BINDIR=bin
INC=-I./$(INCDIR)

CXX=g++
CC=gcc
CXXFLAGS=-std=c++11 `llvm-config --cxxflags` $(INC)
CFLAGS=-w $(INC)
LDFLAGS=`llvm-config --ldflags --system-libs --libs all` /usr/lib/x86_64-linux-gnu/libfl.a

default: $(BINDIR)/alan $(LIBDIR)/libalanstd.a

$(BUILDDIR)/lexer.cpp: $(SRCDIR)/lexer.l
	mkdir -p $(BUILDDIR)
	flex -s -o $(BUILDDIR)/lexer.cpp $(SRCDIR)/lexer.l

$(BUILDDIR)/lexer.o: $(BUILDDIR)/lexer.cpp $(BUILDDIR)/parser.hpp $(INCDIR)/ast.hpp
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -w -o $@ -c $<

$(BUILDDIR)/parser.hpp $(BUILDDIR)/parser.cpp: $(SRCDIR)/parser.ypp
	mkdir -p $(BUILDDIR)
	bison -dv -o $(BUILDDIR)/parser.cpp $(SRCDIR)/parser.ypp

$(BUILDDIR)/symbol.o     : $(SRCDIR)/symbol.cpp $(INCDIR)/symbol.hpp $(INCDIR)/general.hpp $(INCDIR)/error.hpp
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -Wno-cast-qual -o $@ -c $<

$(BUILDDIR)/%.o: $(SRCDIR)/%.cpp
	mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -o $@ -c $<	

$(BUILDDIR)/libalanstd.o : $(SRCDIR)/libalanstd.c
	mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ -c $<

$(LIBDIR)/libalanstd.a : $(BUILDDIR)/libalanstd.o
	mkdir -p $(LIBDIR)
	ar rvs $@ $<

$(BINDIR)/alan: $(BUILDDIR)/lexer.o $(BUILDDIR)/parser.o $(BUILDDIR)/ast.o $(BUILDDIR)/codegen.o $(BUILDDIR)/symbol.o $(BUILDDIR)/error.o $(BUILDDIR)/general.o
	mkdir -p $(BINDIR)
	$(CXX) $(CXXFLAGS) -o $(BINDIR)/alan $^ $(LDFLAGS)

clean:
	$(RM) -rf $(BUILDDIR) $(LIBDIR)

distclean: clean
	$(RM) -rf $(BINDIR)
