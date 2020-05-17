@echo off

IF NOT EXIST build mkdir build
pushd build
cl /Zi /WX ../source/generator.c /link user32.lib /out:sitegen.exe
popd
