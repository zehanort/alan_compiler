#!/bin/bash

for dir in $(find -iname should_run); do
	for infile in $(ls $dir/*.alan); do

		echo " === checking file $infile ==="

		./alanc $infile

		INPUTFILE=$dir/$(basename $infile .alan).stdin
		OUTPUTFILE=$dir/$(basename $infile .alan).stdout

		# *.stdin file does not exist; just compare output to *.stdout file
		if [ ! -f $INPUTFILE ]; then
			diff $OUTPUTFILE <(./a.out)
			continue
		fi

		# *.stdin file exists; feed it to a.out and then compare output to *.stdout file
		diff $OUTPUTFILE <(./a.out < $INPUTFILE)

	done
done

rm a.out
