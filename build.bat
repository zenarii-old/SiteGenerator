@echo off

IF NOT EXIST build mkdir build
pushd build
clang -Werror -g -gcodeview ../source/generator.c -o sitegen.exe -target x86_64-w64-mingw64
popd
echo Compilation completed at %time%.
