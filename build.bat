@echo off
cls
IF NOT EXIST build mkdir build
IF NOT EXIST sites mkdir sites
pushd build
clang -Werror -g -gcodeview ../source/generator.c -o sitegen.exe -target x86_64-w64-mingw64
popd
robocopy build sites sitegen.exe > nul
echo Compilation completed at %time%.
