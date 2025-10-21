#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "util.h"



// 計算連續相同字元的數量
int continuous_count(const char *p){
    int count = 0;
    char first = *p;
    while (*p == first && *p != '\0') {
        count++;
        p++;
    }
    return count;
}

//Clear loops & Copy loops & Multiplication loops optimization
int check_loops(char *p,int *index,int *mult){
    int res,offset=0,_index=0;
    if (*(p+1)!='-') return -1;
    p+=2;
    while (*p!=']'){
        if (*p=='[' || *p=='-' || *p=='.' || *p==',') {
            return -1;
        }
        res=continuous_count(p);
        if (*p=='>') {
            offset+=res;
        }
        else if (*p=='<') {
            offset-=res;
        }
        else if (*p=='+'){
            index[_index]=offset;
            mult[_index]=res;
            _index++;
        }
        p+=res;
    }
    if (offset!=0) return -1;
    return _index;
}


void interpret(const char *const input){

    //initialize the tape with 30000 zeroes
    uint8_t tape[30000]={0};

    //set the pointer to the left most cell of the tape
    uint8_t *ptr=tape;

    char current_char;
    for (int i=0;(current_char=input[i])!='\0';++i){
        switch (current_char) {
            case '>':
                {
                    int count = continuous_count(&input[i]);
                    ptr += count;
                    i += count - 1;  // -1 因為 for 迴圈會 ++i
                    // ++ptr;
                    break;
                }
            case '<':
                {
                    int count = continuous_count(&input[i]);
                    ptr -= count;
                    i += count - 1;
                    // --ptr;
                break;
                }
            case '+':
            {
                
                int count = continuous_count(&input[i]);
                *ptr += count;  // 直接加，不用迴圈
                i += count - 1;
                
                break;
            }
            case '-':{
               
                int count=continuous_count(&input[i]);
                *ptr -= count;  // 直接減，不用迴圈
                i += count - 1;
                
                break;
            }
            case '.':
                putchar(*ptr);
                break;
            case ',':
                *ptr=getchar();
                break;
            case '[':
                // 嘗試優化循環（清零、複製、乘法循環）
                {
                    int index[100], mult[100];
                    int loop_info = check_loops((char*)&input[i], index, mult);
                    
                    if (loop_info >= 0 && *ptr != 0) {
                        // 可以優化的循環
                        for (int j = 0; j < loop_info; j++) {
                            ptr[index[j]] += *ptr * mult[j];
                        }
                        *ptr = 0;  // 清零當前單元格
                        
                        // 跳到對應的 ]
                        int loop = 1;
                        while (loop > 0) {
                            current_char = input[++i];
                            if (current_char == ']') {
                                --loop;
                            }
                            else if (current_char == '[') {
                                ++loop;
                            }
                        }
                    }
                    else if (!(*ptr)) {
                        // 如果當前單元格為0，跳過整個循環
                        int loop = 1;
                        while (loop > 0) {
                            current_char = input[++i];
                            if (current_char == ']') {
                                --loop;
                            }
                            else if (current_char == '[') {
                                ++loop;
                            }
                        }
                    }
                    // 否則正常執行循環（不做任何事，讓程式繼續執行）
                }
                break;

            case ']':
                if (*ptr){
                    int loop=1;
                    while (loop>0){
                        current_char=input[--i];
                        if (current_char=='['){
                            --loop;
                        }
                        else if (current_char==']'){
                            ++loop;
                        }
                    }
                }
                break;
        }
    }
}

int main(int argc,char *argv[]){
    if (argc!=2) {
        err("Usage: interpreter <inputfile>");
    }

    char *file_content=read_file(argv[1]);
    if (file_content==NULL){
        err("can't open file");
    }
    interpret(file_content);
    free(file_content);

    return 0;
}