#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "util.h"

void interpret(const char *const input){

    //initialize the tape with 30000 zeroes
    uint8_t tape[30000]={0};

    //set the pointer to the left most cell of the tape
    uint8_t *ptr=tape;

    char current_char;
    for (int i=0;(current_char=input[i])!='\0';++i){
        switch (current_char) {
            case '>':
                ++ptr;
                break;
            case '<':
                --ptr;
                break;
            case '+':
                ++(*ptr);
                break;
            case '-':
                --(*ptr);
                break;
            case '.':
                putchar(*ptr);
                break;
            case ',':
                *ptr=getchar();
                break;
            case '[':
                if (!(*ptr)){
                    int loop=1;
                    while (loop>0){
                        current_char=input[++i];
                        if (current_char==']'){
                            --loop;
                        }
                        else if (current_char=='['){
                            ++loop;
                        }
                    }
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