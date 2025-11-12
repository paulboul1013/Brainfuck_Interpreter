#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../util.h"

static int count_eliminate=0;

//備註:brainfuck做dead code elimination有點多餘，因為根本不會有dead code，算是二次驗證
char *eliminate_dead_code(char *text_body) {
    size_t text_len=strlen(text_body);
    char *opt=malloc(text_len+1);
    if (opt==NULL) {
        fprintf(stderr,"Memory allocation failed\n");
        exit(1);
    }

    size_t write_pos=0;

    for (size_t i=0;i<text_len;i++) {
        char current=text_body[i];

        //非brainfuck指令，直接跳過
        if (current!='+' && current!='-' &&
            current != '>' && current != '<' && 
            current != '.' && current != ',' && 
            current != '[' && current != ']') {
                continue;
        }

        //檢查上一個指令是否能抵銷
        if (write_pos > 0 ){
            char prev=opt[write_pos-1];

            if ((prev=='+' && current=='-') ||
                (prev=='-' && current=='+') ||
                (prev=='>' && current=='<') ||
                (prev=='<' && current=='>')) {
                    write_pos--;
                    count_eliminate++;
                    continue;
                }
        }

        opt[write_pos++]=current;
    }

    opt[write_pos]='\0';
    return opt;

} 


void compiler(const char * const text_body) {
    int num_brackets=0;
    int matching_brackets=0;
    struct stack stack={.size=0,.items={0}};
    int register_counter=0; //llvm virtual register counter

    //llvm IR code generation
    const char * const prologue=
    "; ModuleID = 'brainfuck'\n"
    "source_filename = \"brainfuck\"\n"
    "target datalayout = \"e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128\"\n"
    "target triple = \"x86_64-pc-linux-gnu\"\n"
    "\n"
    "; 聲明外部 C 函數\n"
    "declare i32 @putchar(i32)\n"
    "declare i32 @getchar()\n"
    "declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i1 immarg)\n"
    "\n"
    "; Brainfuck 主函數\n"
    "define dso_local i32 @main() {\n"
    "entry:\n"
    "  ; 分配 30,000 字節的記憶體數組\n"
    "  %memory = alloca [30000 x i8], align 16\n"
    "  \n"
    "  ; 初始化記憶體為 0\n"
    "  %memory_ptr = bitcast [30000 x i8]* %memory to i8*\n"
    "  call void @llvm.memset.p0i8.i64(i8* align 16 %memory_ptr, i8 0, i64 30000, i1 false)\n"
    "  \n"
    "  ; 初始化指標為 0\n"
    "  %ptr = alloca i32, align 4\n"
    "  store i32 0, i32* %ptr, align 4\n"
    "  \n"
    "  br label %bf_code\n"
    "\n"
    "bf_code:\n";

    puts(prologue);

    for (unsigned long i=0;text_body[i]!='\0';i++) {
        
        int count=1;
        char current=text_body[i];

        //combine consecutive instructions
        if (current=='+' || current == '-' || current == '>' || current=='<'){
            while (text_body[i+1]==current){
                count++;
                i++;
            }
        }


        switch (current) {
            case '>':
                {
                    int r1 = register_counter++;
                    int r2 = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r1);
                    printf("  %%r%d = add i32 %%r%d, %d\n", r2, r1, count);
                    printf("  store i32 %%r%d, i32* %%ptr, align 4\n", r2);
                }
                break;
            case '<':
                {
                    int r1 = register_counter++;
                    int r2 = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r1);
                    printf("  %%r%d = sub i32 %%r%d, %d\n", r2, r1, count);
                    printf("  store i32 %%r%d, i32* %%ptr, align 4\n", r2);
                }
                break;
            case '+':
                {
                    int r_ptr = register_counter++;
                    int r_cellptr = register_counter++;
                    int r_val = register_counter++;
                    int r_new = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r_ptr);
                    printf("  %%r%d = getelementptr inbounds [30000 x i8], [30000 x i8]* %%memory, i32 0, i32 %%r%d\n", r_cellptr, r_ptr);
                    printf("  %%r%d = load i8, i8* %%r%d\n", r_val, r_cellptr);
                    printf("  %%r%d = add i8 %%r%d, %d\n", r_new, r_val, count);
                    printf("  store i8 %%r%d, i8* %%r%d\n", r_new, r_cellptr);
                }
                break;
            case '-':
                {
                    int r_ptr = register_counter++;
                    int r_cellptr = register_counter++;
                    int r_val = register_counter++;
                    int r_new = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r_ptr);
                    printf("  %%r%d = getelementptr inbounds [30000 x i8], [30000 x i8]* %%memory, i32 0, i32 %%r%d\n", r_cellptr, r_ptr);
                    printf("  %%r%d = load i8, i8* %%r%d\n", r_val, r_cellptr);
                    printf("  %%r%d = sub i8 %%r%d, %d\n", r_new, r_val, count);
                    printf("  store i8 %%r%d, i8* %%r%d\n", r_new, r_cellptr);
                }
                break;
            case '.':
                {
                    int r_ptr = register_counter++;
                    int r_cellptr = register_counter++;
                    int r_val = register_counter++;
                    int r_ext = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r_ptr);
                    printf("  %%r%d = getelementptr inbounds [30000 x i8], [30000 x i8]* %%memory, i32 0, i32 %%r%d\n", r_cellptr, r_ptr);
                    printf("  %%r%d = load i8, i8* %%r%d\n", r_val, r_cellptr);
                    printf("  %%r%d = zext i8 %%r%d to i32\n", r_ext, r_val);
                    printf("  call i32 @putchar(i32 %%r%d)\n", r_ext);
                }
                break;
            case ',':
                {
                    int r_in = register_counter++;
                    int r_tr = register_counter++;
                    int r_ptr = register_counter++;
                    int r_cellptr = register_counter++;
                    printf("  %%r%d = call i32 @getchar()\n", r_in);
                    printf("  %%r%d = trunc i32 %%r%d to i8\n", r_tr, r_in);
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r_ptr);
                    printf("  %%r%d = getelementptr inbounds [30000 x i8], [30000 x i8]* %%memory, i32 0, i32 %%r%d\n", r_cellptr, r_ptr);
                    printf("  store i8 %%r%d, i8* %%r%d\n", r_tr, r_cellptr);
                }
                break;
            case '[':
                {
                    int loop_id = num_brackets++;
                    if (stack_push(&stack, loop_id) != 0) {
                        err("Loop stack overflow");
                    }
                    printf("  br label %%bf_loop_check%d\n", loop_id);
                    printf("\n");
                    printf("bf_loop_check%d:\n", loop_id);
                    int r_ptr = register_counter++;
                    int r_cellptr = register_counter++;
                    int r_val = register_counter++;
                    int r_cmp = register_counter++;
                    printf("  %%r%d = load i32, i32* %%ptr, align 4\n", r_ptr);
                    printf("  %%r%d = getelementptr inbounds [30000 x i8], [30000 x i8]* %%memory, i32 0, i32 %%r%d\n", r_cellptr, r_ptr);
                    printf("  %%r%d = load i8, i8* %%r%d\n", r_val, r_cellptr);
                    printf("  %%r%d = icmp ne i8 %%r%d, 0\n", r_cmp, r_val);
                    printf("  br i1 %%r%d, label %%bf_loop_body%d, label %%bf_loop_end%d\n", r_cmp, loop_id, loop_id);
                    printf("\n");
                    printf("bf_loop_body%d:\n", loop_id);
                }
                break;
            case ']':
                {
                    int loop_id = -1;
                    if (stack_pop(&stack, &loop_id) != 0) {
                        err("Unmatched closing bracket ']'");
                    }
                    printf("  br label %%bf_loop_check%d\n", loop_id);
                    printf("\n");
                    printf("bf_loop_end%d:\n", loop_id);
                }
                break;
            default:
                // 已於 eliminate_dead_code 過濾非指令，這裡可忽略
                break;
        }
    }
    if (stack.size != 0) {
        err("Unmatched opening bracket '['");
    }
    printf("  ret i32 0\n");
    printf("}\n");
}

int main(int argc, char *argv[]) {
    
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <input file> \n",argv[0]);
        exit(1);
    }

    char *text_body=read_file(argv[1]);
    if (text_body==NULL) {
        fprintf(stderr, "Could not read input file %s \n",argv[1]);
        exit(1);
    }

    char *optimized_text=eliminate_dead_code(text_body);
    free(text_body);


    // printf("Eliminated %d instructions\n",count_eliminate);


    compiler(optimized_text);


    return 0;
}