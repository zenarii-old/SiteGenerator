#!/bin/bash

code="$PWD"
opts=-g
cd build > /dev/null
gcc $opts $code/source/main.c -o $code/build/sitegen -Wall -Wextra -Werror -Wno-unused-function
cd $code > /dev/null
