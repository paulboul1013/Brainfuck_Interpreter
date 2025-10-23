#include <stdio.h>
#include <stdlib.h>
#include "util.h"


void compile(const char * const text_body){
    int num_brackets=0;
    int matching_brackets=0;
    struct stack stack={.size=0,.items={0}};

    // 使用系統調用，不依賴 C 庫
    const char * const prologue=
        ".section .note.GNU-stack,\"\",%progbits\n"
        ".section .data\n"
        "memory: .skip 30000\n"          // 靜態分配記憶體
        ".section .text\n"
        ".global _start\n"
        "_start:\n"
        "    leal memory, %ecx\n";       // ecx 指向記憶體起始
    puts(prologue);

    for(unsigned long i=0;text_body[i]!='\0';++i){
        switch (text_body[i]){
            case '>':
                puts("    incl %ecx");
                break;
            case '<':
                puts("    decl %ecx");
                break;
            case '+':
                puts("    incb (%ecx)");
                break;
            case '-':
                puts("    decb (%ecx)");
                break;
            case '.':
                // 使用 sys_write 系統調用
                puts("    movl $4, %eax");      // sys_write
                puts("    movl $1, %ebx");      // stdout
                puts("    movl %ecx, %edx");    // 指向字元
                puts("    pushl %ecx");         // 保存 ecx
                puts("    movl %edx, %ecx");    // 數據指針
                puts("    movl $1, %edx");      // 長度 = 1
                puts("    int $0x80");
                puts("    popl %ecx");          // 恢復 ecx
                break;
            case ',':
                // 使用 sys_read 系統調用
                puts("    movl $3, %eax");      // sys_read
                puts("    movl $0, %ebx");      // stdin
                puts("    pushl %ecx");         // 保存 ecx
                puts("    movl %ecx, %edx");    // 數據指針
                puts("    movl %edx, %ecx");    // 數據指針
                puts("    movl $1, %edx");      // 長度 = 1
                puts("    int $0x80");
                puts("    popl %ecx");          // 恢復 ecx
                break;
            case '[':
                if (stack_push(&stack,num_brackets)==0){
                    puts  ("    cmpb $0, (%ecx)");
				    printf("    je bracket_%d_end\n", num_brackets);
				    printf("bracket_%d_start:\n", num_brackets++);
                } else {
                    err("out of stack space");
                }
                break;
            case ']':
                if (stack_pop(&stack,&matching_brackets)==0){
                    puts  ("    cmpb $0, (%ecx)");
                    printf("    jne bracket_%d_start\n", matching_brackets);
                    printf("bracket_%d_end:\n", matching_brackets);
                } else {
                    err("stack underflow, unmatched");
                }
                break;
        }
    }

    // 使用 sys_exit 退出
    const char * const epilogue=
        "    movl $1, %eax\n"      // sys_exit
        "    xorl %ebx, %ebx\n"    // 退出碼 = 0
        "    int $0x80\n";
    puts(epilogue);
}

int main(int argc,char *argv[]){
    if (argc!=2){
        err("Usage: compiler_x86 inputfile");
    }
    char *text_body=read_file(argv[1]);
    if (text_body==NULL){
        err("Unable to read program text file");
    }

    compile(text_body);

    free(text_body);
    return 0;
}

