#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

// 檢測並展開簡單迴圈
// 返回值：如果可以展開，返回跳過的字符數；否則返回 0
int try_unroll_loop_x86(const char *text_body, unsigned long start_pos) {
    if (text_body[start_pos] != '[') return 0;
    
    unsigned long pos = start_pos + 1;
    unsigned long loop_start = pos;
    int depth = 1;
    
    // 找到對應的 ]
    while (depth > 0 && text_body[pos] != '\0') {
        if (text_body[pos] == '[') depth++;
        else if (text_body[pos] == ']') depth--;
        pos++;
    }
    
    if (depth != 0) return 0; // 沒有找到匹配的 ]
    
    unsigned long loop_end = pos - 1;
    unsigned long loop_len = loop_end - loop_start;
    
    // 檢查是否為 [-], [+], [>], [<] 等簡單模式
    if (loop_len == 1) {
        char op = text_body[loop_start];
        
        // [-] 或 [+] - 清零迴圈
        if (op == '-' || op == '+') {
            puts("    movb $0, (%ecx)");
            return pos - start_pos; // 返回跳過的字符數
        }
        
        // [>] 或 [<] - 掃描迴圈
        if (op == '>') {
            printf("loop_scan_right_%lu:\n", start_pos);
            puts("    cmpb $0, (%ecx)");
            printf("    je loop_scan_right_%lu_end\n", start_pos);
            puts("    incl %ecx");
            printf("    jmp loop_scan_right_%lu\n", start_pos);
            printf("loop_scan_right_%lu_end:\n", start_pos);
            return pos - start_pos;
        }
        
        if (op == '<') {
            printf("loop_scan_left_%lu:\n", start_pos);
            puts("    cmpb $0, (%ecx)");
            printf("    je loop_scan_left_%lu_end\n", start_pos);
            puts("    decl %ecx");
            printf("    jmp loop_scan_left_%lu\n", start_pos);
            printf("loop_scan_left_%lu_end:\n", start_pos);
            return pos - start_pos;
        }
    }
    
    // 檢查連續的 > 或 <（如 [>>>], [<<<]）
    if (loop_len > 1) {
        char first = text_body[loop_start];
        int can_unroll = 1;
        
        for (unsigned long i = loop_start; i < loop_end; i++) {
            if (text_body[i] != first) {
                can_unroll = 0;
                break;
            }
        }
        
        if (can_unroll && (first == '>' || first == '<')) {
            int step = loop_len;
            
            if (first == '>') {
                printf("loop_scan_right_%lu:\n", start_pos);
                puts("    cmpb $0, (%ecx)");
                printf("    je loop_scan_right_%lu_end\n", start_pos);
                printf("    addl $%d, %%ecx\n", step);
                printf("    jmp loop_scan_right_%lu\n", start_pos);
                printf("loop_scan_right_%lu_end:\n", start_pos);
            } else {
                printf("loop_scan_left_%lu:\n", start_pos);
                puts("    cmpb $0, (%ecx)");
                printf("    je loop_scan_left_%lu_end\n", start_pos);
                printf("    subl $%d, %%ecx\n", step);
                printf("    jmp loop_scan_left_%lu\n", start_pos);
                printf("loop_scan_left_%lu_end:\n", start_pos);
            }
            return pos - start_pos;
        }
    }
    
    return 0; // 無法展開
}

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
                    puts("    incl %ecx");
                } else {
                    printf("    addl $%d, %%ecx\n", count);
                }
                break;
            case '<':
                if (count == 1) {
                    puts("    decl %ecx");
                } else {
                    printf("    subl $%d, %%ecx\n", count);
                }
                break;
            case '+':
                if (count == 1) {
                    puts("    incb (%ecx)");
                } else {
                    printf("    addb $%d, (%%ecx)\n", count);
                }
                break;
            case '-':
                if (count == 1) {
                    puts("    decb (%ecx)");
                } else {
                    printf("    subb $%d, (%%ecx)\n", count);
                }
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
                {
                    // 嘗試迴圈展開優化
                    int skipped = try_unroll_loop_x86(text_body, i);
                    if (skipped > 0) {
                        // 迴圈已展開，跳過整個迴圈
                        i += skipped - 1; // -1 因為 for 迴圈會 ++i
                        break;
                    }
                    
                    // 無法展開，使用正常迴圈生成
                    if (stack_push(&stack,num_brackets)==0){
                        puts  ("    cmpb $0, (%ecx)");
                        printf("    je bracket_%d_end\n", num_brackets);
                        printf("bracket_%d_start:\n", num_brackets++);
                    } else {
                        err("out of stack space");
                    }
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

