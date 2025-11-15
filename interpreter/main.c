#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "../util.h"
#include <ctype.h>



// 除錯輸出：顯示指令計數器、當前指令、資料指標與記憶體視窗
static void debug_print_state(int ip, char instr, const uint8_t *tape, const uint8_t *ptr, int window){
    int dp = (int)(ptr - tape);
    if (dp < 0) dp = 0;
    if (dp > 29999) dp = 29999;
    int start = dp - window;
    int end = dp + window;
    if (start < 0) start = 0;
    if (end > 29999) end = 29999;

	// 產生可讀的字元顯示
	char chbuf[8];
	const char *chrepr;
	unsigned v = (unsigned)*ptr;
	if (v == 0) {
		chrepr = "\\0";
	} else if (v == '\n') {
		chrepr = "\\n";
	} else if (v == '\r') {
		chrepr = "\\r";
	} else if (v == '\t') {
		chrepr = "\\t";
	} else if (isprint((int)v)) {
		chbuf[0] = (char)v;
		chbuf[1] = '\0';
		chrepr = chbuf;
	} else {
		snprintf(chbuf, sizeof(chbuf), "\\x%02X", v & 0xFFu);
		chrepr = chbuf;
	}

	fprintf(stderr, "[DEBUG] ip=%d instr=%c dp=%d val=%u ch='%s' | tape[%d..%d]: ",
	        ip, instr, dp, v, chrepr, start, end);
    for (int i = start; i <= end; ++i){
        if (i == dp){
            fprintf(stderr, "[%u] ", (unsigned)tape[i]);
        }else{
            fprintf(stderr, "%u ", (unsigned)tape[i]);
        }
    }
    fprintf(stderr, "\n");
}


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

// Scan loops optimization: 檢測 [>]、[<]、[>>>] 等模式
// 返回值：0 表示不是 scan loop，正數表示向右移動步數，負數表示向左移動步數
int check_scan_loop(const char *p) {
    if (*p != '[') return 0;
    p++; // 跳過 '['
    
    // 檢查循環內只有 > 或 <
    if (*p != '>' && *p != '<') return 0;
    
    char direction = *p;
    int step = 0;
    
    while (*p == direction) {
        step++;
        p++;
    }
    
    // 必須以 ] 結束，且中間沒有其他指令
    if (*p != ']') return 0;
    
    // 返回移動步數（正數向右，負數向左）
    return (direction == '>') ? step : -step;
}

// 移除換行符號和非指令字符
// 只保留 Brainfuck 的有效指令：><+-.,[]
// 這樣可以避免連續指令被換行截斷
char* remove_non_instructions(const char *input) {
    if (input == NULL) return NULL;
    
    // 計算需要的空間（最多與原字串相同長度）
    size_t len = strlen(input);
    char *output = (char*)malloc(len + 1);
    if (output == NULL) return NULL;
    
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        char c = input[i];
        // 只保留有效的 Brainfuck 指令
        if (c == '>' || c == '<' || c == '+' || c == '-' || 
            c == '.' || c == ',' || c == '[' || c == ']') {
            output[j++] = c;
        }
        // 其他字符（包括換行、空格、註解等）都忽略
    }
    output[j] = '\0';
    
    return output;
}


void interpret(const char *const input, int debug, int debug_window){

    //initialize the tape with 30000 zeroes
    uint8_t tape[30000]={0};

    //set the pointer to the left most cell of the tape
    uint8_t *ptr=tape;

    char current_char;
	int last_stdout_char = '\n'; // 追蹤最近一次 stdout 字元，預設視為已換行
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
				fflush(stdout);
				last_stdout_char = *ptr;
                break;
            case ',':
				*ptr=getchar();
                break;
            case '[':
                {
                    // 先檢查是否為 Scan loop ([>], [<], [>>>] 等)
                    int scan_step = check_scan_loop(&input[i]);
                    
                    if (scan_step != 0 && *ptr != 0) {
                        // Scan loop 優化
                        if (scan_step > 0) {
                            // 向右掃描 [>], [>>] 等
                            // 使用 memchr 快速找到值為 0 的位置
                            uint8_t *result;
                            uint8_t *search_ptr = ptr;
                            const uint8_t *tape_end = tape + 30000;
                            
                            while (search_ptr < tape_end) {
                                size_t remaining = tape_end - search_ptr;
                                result = memchr(search_ptr, 0, remaining);
                                if (result == NULL) {
                                    // 沒找到，移動到最後
                                    ptr = (uint8_t*)tape_end - 1;
                                    break;
                                }
                                
                                // 檢查是否在正確的步長位置上
                                ptrdiff_t offset = result - ptr;
                                if (offset % scan_step == 0) {
                                    ptr = result;
                                    break;
                                }
                                // 否則繼續從下一個位置搜尋
                                search_ptr = result + 1;
                            }
                        } else {
                            // 向左掃描 [<], [<<] 等
                            int step = -scan_step;
                            const uint8_t *tape_start = tape;
                            
                            // 反向查找第一個 0（按步長）
                            while (ptr > tape_start && *ptr != 0) {
                                ptr -= step;
                                if (ptr < tape_start) {
                                    ptr = (uint8_t*)tape_start;
                                    break;
                                }
                            }
                        }
                        
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
                    else {
                        // 嘗試優化循環（清零、複製、乘法循環）
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
        if (debug){
			// 確保除錯輸出與正常輸出分行，避免混寫在同一列
			if (last_stdout_char != '\n') {
				fputc('\n', stderr);
				last_stdout_char = '\n';
			}
			debug_print_state(i, current_char, tape, ptr, debug_window);
        }
    }
}

int main(int argc,char *argv[]){
    int debug = 0;
	int debug_window = 8;
    const char *filepath = NULL;

	// 參數解析：支援 -d/--debug 與 -w/--debug-window <N> 以及檔名
    for (int i = 1; i < argc; ++i){
        if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0){
            debug = 1;
		}else if ((strcmp(argv[i], "-w") == 0 || strcmp(argv[i], "--debug-window") == 0) && (i + 1 < argc)){
			int w = atoi(argv[++i]);
			if (w < 1) w = 1;
			if (w > 64) w = 64; // 合理上限
			debug_window = w;
        }else if (filepath == NULL){
            filepath = argv[i];
        }else{
			err("Usage: interpreter [-d|--debug] [-w|--debug-window N] <inputfile>");
        }
    }
    if (filepath == NULL){
		err("Usage: interpreter [-d|--debug] [-w|--debug-window N] <inputfile>");
    }

    char *file_content=read_file(filepath);
    if (file_content==NULL){
        err("can't open file");
    }
    
    // 移除換行符號和非指令字符，避免連續指令被截斷
    char *cleaned_content = remove_non_instructions(file_content);
    free(file_content);
    
    if (cleaned_content == NULL) {
        err("memory allocation failed");
    }
    
	interpret(cleaned_content, debug, debug_window);
    free(cleaned_content);

    return 0;
}