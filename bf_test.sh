#!/bin/bash

# test compiler_x86
gcc -o compiler_x86 compiler_x86.c
./compiler_x86 mandelbrot.b > mandelbrot.s
gcc -m32 -nostdlib -no-pie -o mandelbrot mandelbrot.s
time ./mandelbrot

# test compiler_x86_64
gcc -o compiler_x86_64 compiler_x86_64.c
./compiler_x86_64 mandelbrot.b > mandelbrot.s
gcc -nostdlib -no-pie -o mandelbrot mandelbrot.s
time ./mandelbrot