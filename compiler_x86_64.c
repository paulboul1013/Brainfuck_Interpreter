#include <stdio.h>
#include <stdlib.h>
#include "util.h"


void compile(const char * const text_body){
    int num_brackets=0;
    int matching_brackets=0;
    struct stack stack={.size=0,.items={0}};

    // 使用 64 位元系統調用，不依賴 C 庫
    const char * const prologue=
        ".section .note.GNU-stack,\"\",%progbits\n"
        ".section .data\n"
        "memory: .skip 30000\n"          // 靜態分配記憶體
        ".section .text\n"
        ".global _start\n"
        "_start:\n"
        "    leaq memory(%rip), %r12\n";  // r12 指向記憶體起始（使用 RIP-relative 尋址）
    puts(prologue);

    for(unsigned long i=0;text_body[i]!='\0';++i){
        // 指令合併優化：計算連續相同指令的數量
        int count = 1;
        char current = text_body[i];
        
        // 只對 +, -, >, < 進行合併優化
        if (current == '+' || current == '-' || current == '>' || current == '<') {
            while (text_body[i+1] == current) {
                count++;
                i++;
            }
        }
        
        switch (current){
            case '>':
                if (count == 1) {
                    puts("    incq %r12");
                } else {
                    printf("    addq $%d, %%r12\n", count);
                }
                break;
            case '<':
                if (count == 1) {
                    puts("    decq %r12");
                } else {
                    printf("    subq $%d, %%r12\n", count);
                }
                break;
            case '+':
                if (count == 1) {
                    puts("    incb (%r12)");
                } else {
                    printf("    addb $%d, (%%r12)\n", count);
                }
                break;
            case '-':
                if (count == 1) {
                    puts("    decb (%r12)");
                } else {
                    printf("    subb $%d, (%%r12)\n", count);
                }
                break;
            case '.':
                // 使用 sys_write 系統調用 (64-bit)
                // rax = 1 (sys_write), rdi = 1 (stdout), rsi = buffer, rdx = count
                puts("    movq $1, %rax");       // sys_write
                puts("    movq $1, %rdi");       // stdout
                puts("    movq %r12, %rsi");     // 指向字元
                puts("    movq $1, %rdx");       // 長度 = 1
                puts("    syscall");
                break;
            case ',':
                // 使用 sys_read 系統調用 (64-bit)
                // rax = 0 (sys_read), rdi = 0 (stdin), rsi = buffer, rdx = count
                puts("    movq $0, %rax");       // sys_read
                puts("    movq $0, %rdi");       // stdin
                puts("    movq %r12, %rsi");     // 指向字元
                puts("    movq $1, %rdx");       // 長度 = 1
                puts("    syscall");
                break;
            case '[':
                if (stack_push(&stack,num_brackets)==0){
                    puts  ("    cmpb $0, (%r12)");
				    printf("    je bracket_%d_end\n", num_brackets);
				    printf("bracket_%d_start:\n", num_brackets++);
                } else {
                    err("out of stack space");
                }
                break;
            case ']':
                if (stack_pop(&stack,&matching_brackets)==0){
                    puts  ("    cmpb $0, (%r12)");
                    printf("    jne bracket_%d_start\n", matching_brackets);
                    printf("bracket_%d_end:\n", matching_brackets);
                } else {
                    err("stack underflow, unmatched");
                }
                break;
        }
    }

    // 使用 sys_exit 退出 (64-bit)
    const char * const epilogue=
        "    movq $60, %rax\n"     // sys_exit (64-bit)
        "    xorq %rdi, %rdi\n"    // 退出碼 = 0
        "    syscall\n";
    puts(epilogue);
}

int main(int argc,char *argv[]){
    if (argc!=2){
        err("Usage: compiler_x86_64 inputfile");
    }
    char *text_body=read_file(argv[1]);
    if (text_body==NULL){
        err("Unable to read program text file");
    }

    compile(text_body);

    free(text_body);
    return 0;
}

