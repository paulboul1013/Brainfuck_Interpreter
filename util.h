#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

//print error message and exit
static inline void err(const char *msg){
    fprintf(stderr,"%s\n",msg);
    exit(1);
}

static inline char *read_file(const char *const filename){
    if (filename ==NULL){
        return NULL;
    }

    FILE *fp=fopen(filename,"r");
    if (fp==NULL){
        return NULL;
    }

    //fp move to the end of the file，using ftell(fp) to get the file size
    assert(!fseek(fp,0,SEEK_END));
    long file_size=ftell(fp);

    //remove to beginning of the file
    rewind(fp);

    size_t code_size=sizeof(char)*(file_size+1);
    char *code=malloc(code_size);

    if (code==NULL){
        return NULL;
    }

    //read the file into the code
    fread(code,1,file_size,fp);
    code[file_size]='\0';
    assert(!fclose(fp));
    return code;
   
}

#define STACKSIZE 100

struct stack{
    int size;
    int items[STACKSIZE];
};

static inline int stack_push(struct stack * const p,const int x){
    if (p->size==STACKSIZE){
        return -1;
    }

    p->items[p->size++]=x;
    return 0;
}

static inline int stack_pop(struct stack * const p,int *x){
    if (p->size==0){
        return -1;
    }

    *x=p->items[--p->size];
    return 0;
}