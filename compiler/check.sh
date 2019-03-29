#!/bin/bash

echo ">>> Compiling..."
echo ""

make clean; make

echo ">>> Done."
echo ""
echo "======================================================="
echo ""

CC=alan
INPUTDIR=../examples/
INPUTS=$(find $INPUTDIR | grep '\.alan')

for INPUT in $INPUTS; do
	echo ">> Checking alan compiler with input $(tput bold)\"$INPUT\"$(tput sgr0):"
	echo ">>>> Output:"
	./$CC < $INPUTDIR$INPUT
	echo ""
done

echo "======================================================="
echo ""

make distclean
