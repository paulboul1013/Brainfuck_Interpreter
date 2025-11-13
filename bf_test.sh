#!/bin/bash

#test interpreter
gcc -I. -o interpreter_main interpreter/main.c
time ./interpreter_main mandelbrot.bf


# test compiler_x86
gcc -I. -o  compiler_x86 compiler_x86_source/compiler_x86.c
./compiler_x86 mandelbrot.bf > mandelbrot.s
gcc -m32 -nostdlib -no-pie -o mandelbrot mandelbrot.s
time ./mandelbrot

# test compiler_x86_64
gcc -I. -o  compiler_x86_64 compiler_x86_source/compiler_x86_64.c
./compiler_x86_64 mandelbrot.bf > mandelbrot.s
gcc -nostdlib -no-pie -o mandelbrot mandelbrot.s
time ./mandelbrot

test compiler_llvm
gcc -I. -o  compiler_llvm llvm/llvm.c
./compiler_llvm mandelbrot.bf > mandelbrot.ll
clang -o mandelbrot mandelbrot.ll
time ./mandelbrot