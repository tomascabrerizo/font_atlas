@echo off

IF NOT EXIST .\build mkdir .\build
pushd .\build
clang -Wall -Werror -Wno-deprecated-declarations ^
      ..\main.c -o bfont.exe -g -fdiagnostics-absolute-paths
popd
