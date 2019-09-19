#!/bin/bash

# checks only for compilation errors (overwrites executables)
# agnostic as to whether the executable, if produced, works as expected

not_working=()

for infile in $(find test/ | grep ".alan"); do
	echo "compiling $infile"
	./alanc -x $infile
	if [[ $? -ne $zero ]]; then
		not_working+=($infile)
	fi
done

# report programs that did not compile
echo ""
echo "programs that did not compile:"
for i in "${not_working[@]}"; do
   echo "$i"
done

rm a.out
