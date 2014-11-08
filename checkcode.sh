#!/bin/bash

# Mike's awesome code-checker utility that checks C source files for projects
# like this for various different code-smells.

clear
echo -e "\e[31m====== Files with C++ comments: ======\e[0m"
grep -nR "//" $@

echo -e "\e[31m====== Files with TODOs: ======\e[0m"
grep -nR "TODO:" $@

echo -e "\e[31m====== Files with lines too long ======\e[0m"
grep -nP '.{80,}' $@
grep -nP '\t{1}.{76,}' $@
grep -nP '\t{2}.{72,}' $@
grep -nP '\t{3}.{68,}' $@
grep -nP '\t{4}.{64,}' $@
grep -nP '\t{5,}.{60,}' $@

echo -e "\e[31m====== Files with whitespace lines ======\e[0m"
grep -nP '^\s+$' $@

echo -e "\e[31m====== Files with space indentation ======\e[0m"
grep -nP '^ +' $@

echo -e "\e[31m====== Lines ending with whitespace ======\e[0m"
grep -nP '^.*\s+$' $@

echo -e "\e[31m====== Make warnings ======\e[0m"
make clean >> /dev/null
make | grep -P "(warning|error):"