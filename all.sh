#!/bin/bash

for test in $@; do
	./alanc $test -a -O
done
