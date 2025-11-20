// 即時 Brainfuck 互動直譯器
// - 逐鍵讀取 Brainfuck 指令，立即執行
// - 正確處理 [] 迴圈（支援跨多次輸入）
// - 每步後即時渲染記憶體視窗與輸出緩衝
// - 逗號 , 指令會消耗下一個鍵作為輸入資料，不會加入程式緩衝

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define TAPE_SIZE 30000
#define VIEW_WIDTH 15
#define PROGRAM_INIT_CAP 1024
#define OUTPUT_INIT_CAP 128

static struct termios original_termios;
static bool raw_mode_enabled = false;

static void disable_raw_mode(void) {
	if (raw_mode_enabled) {
		tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
		raw_mode_enabled = false;
	}
}

static void enable_raw_mode(void) {
	if (tcgetattr(STDIN_FILENO, &original_termios) == -1) {
		perror("tcgetattr");
		exit(1);
	}
	atexit(disable_raw_mode);

	struct termios raw = original_termios;
	raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 無回顯、非行模式、忽略 Ctrl-C 等訊號
	raw.c_iflag &= ~(IXON | ICRNL);         // 關閉 Ctrl-S/Q、避免 Enter 轉 CRLF
	// raw.c_oflag &= ~(OPOST);             // 註解掉：保留輸出處理，讓 \n 自動轉換為 \r\n 以便正確對齊

	if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
		perror("tcsetattr");
		exit(1);
	}
	raw_mode_enabled = true;
}

static volatile sig_atomic_t should_exit = 0;
static void handle_signal(int sig) {
	(void)sig;
	should_exit = 1;
}

static inline bool is_bf(char c) {
	return c=='>' || c=='<' || c=='+' || c=='-' || c=='.' || c==',' || c=='[' || c==']';
}

static ssize_t find_matching_forward(const char* program, size_t prog_len, size_t open_idx) {
	if (open_idx >= prog_len || program[open_idx] != '[') return -1;
	int depth = 0;
	for (size_t i = open_idx; i < prog_len; ++i) {
		if (program[i] == '[') depth++;
		else if (program[i] == ']') {
			depth--;
			if (depth == 0) return (ssize_t)i;
		}
	}
	return -1; // 尚未輸入到對應的 ']'
}

static ssize_t find_matching_backward(const char* program, size_t prog_len, size_t close_idx) {
	if (close_idx >= prog_len || program[close_idx] != ']') return -1;
	int depth = 0;
	for (ssize_t i = (ssize_t)close_idx; i >= 0; --i) {
		if (program[i] == ']') depth++;
		else if (program[i] == '[') {
			depth--;
			if (depth == 0) return i;
		}
	}
	return -1;
}

static void append_char(char** buf, size_t* len, size_t* cap, char c) {
	if (*cap == 0) {
		*cap = 64;
		*buf = (char*)malloc(*cap);
		if (!*buf) { perror("malloc"); exit(1); }
	}
	if (*len + 1 >= *cap) {
		size_t new_cap = (*cap) * 2;
		char* n = (char*)realloc(*buf, new_cap);
		if (!n) { perror("realloc"); exit(1); }
		*buf = n;
		*cap = new_cap;
	}
	(*buf)[(*len)++] = c;
}

static unsigned char read_input_byte(void) {
	unsigned char ch = 0;
	ssize_t r = read(STDIN_FILENO, &ch, 1);
	if (r <= 0) return 0;
	return ch;
}

static void render_state(const unsigned char* tape, size_t ptr, size_t ip,
                         const char* program, size_t prog_len,
                         const char* out_buf, size_t out_len) {
	// 清螢幕並回到左上
	write(STDOUT_FILENO, "\033[2J\033[H", 7);

	// 標題與操作說明
	printf("即時 Brainfuck 直譯器 (互動模式)\n");
	printf("輸入 Brainfuck 指令：><+-.,[] ；按 ESC 或 Ctrl-C 離開。\n");
	printf("按 Space 清除所有輸出顯示狀態 \n逗號 , 指令會讀取下一個按鍵作為輸入資料。\n\n");

	// 程式片段顯示（尾端最多 50 字元）
	size_t show_n = prog_len > 50 ? 50 : prog_len;
	printf("Program (%zu chars, IP=%zu): ", prog_len, ip);
	if (show_n < prog_len) printf("..."); // 程式過長時省略前綴
	for (size_t i = prog_len - show_n; i < prog_len; ++i) {
		putchar(program[i]);
	}
	putchar('\n');

	// 輸出緩衝顯示（尾端最多 20 字元）
	size_t out_show_n = out_len > 20 ? 20 : out_len;
	printf("Output (length: %zu): ", out_len);
	if (out_show_n < out_len) printf("..."); 
	for (size_t i = out_len - out_show_n; i < out_len; ++i) {
		unsigned char c = (unsigned char)out_buf[i];
		if (isprint(c)) putchar(c);
		else printf("\\x%02X", c);
	}
	putchar('\n');

	// 記憶體視窗
	printf("\nMemory Position(ptr=%zu):\n", ptr);
	size_t total_view = VIEW_WIDTH;
	if (total_view > TAPE_SIZE) total_view = TAPE_SIZE;

	// 每 10 個單位一組
	for (size_t start = 0; start < total_view; start += 10) {
		size_t end = start + 10;
		if (end > total_view) end = total_view;

		// 1. Idx 列
		printf("Idx: ");
		for (size_t i = start; i < end; ++i) {
			printf("%3zu ", i);
		}
		putchar('\n');

		// 2. Value 列
		for(int i=0; i<5; i++) printf(" "); // 縮排對齊 "Idx: "
		for (size_t i = start; i < end; ++i) {
			printf("%3u ", (unsigned)tape[i]);
		}
		putchar('\n');

		// 3. 指標列
		for(int i=0; i<5; i++) printf(" "); // 縮排對齊 "Idx: "
		for (size_t i = start; i < end; ++i) {
			if (i == ptr) printf("  ^ ");
			else printf("    ");
		}
		printf("\n\n"); // 區塊間隔
	}

	fflush(stdout);
}

int main(void) {
	enable_raw_mode();
	signal(SIGINT, handle_signal);
	signal(SIGTERM, handle_signal);

	unsigned char* tape = (unsigned char*)calloc(TAPE_SIZE, 1);
	if (!tape) { perror("calloc"); return 1; }
	size_t ptr = 0;

	char* program = (char*)malloc(PROGRAM_INIT_CAP);
	size_t prog_len = 0;
	size_t prog_cap = program ? PROGRAM_INIT_CAP : 0;
	if (!program) { perror("malloc"); return 1; }

	char* out_buf = (char*)malloc(OUTPUT_INIT_CAP);
	size_t out_len = 0;
	size_t out_cap = out_buf ? OUTPUT_INIT_CAP : 0;
	if (!out_buf) { perror("malloc"); return 1; }

	size_t ip = 0; // 下一個要執行的指令索引

	// 初始畫面
	render_state(tape, ptr, ip, program, prog_len, out_buf, out_len);

	while (!should_exit) {
		unsigned char ch = 0;
		ssize_t r = read(STDIN_FILENO, &ch, 1);
		if (r <= 0) continue;

		// ESC 或 Ctrl-C 離開
		if (ch == 27 /* ESC */) break;
		if (ch == 3 /* Ctrl-C */) break;

		// Space (ASCII 32) 清除所有狀態
		if (ch == 32) {
			memset(tape, 0, TAPE_SIZE);
			ptr = 0;
			prog_len = 0;
			out_len = 0;
			ip = 0;
			render_state(tape, ptr, ip, program, prog_len, out_buf, out_len);
			continue;
		}

		if (!is_bf((char)ch)) {
			// 忽略非 BF 指令鍵（例如空白、英數等）
			continue;
		}

		// 將新指令加入程式緩衝
		if (prog_len + 1 >= prog_cap) {
			size_t new_cap = prog_cap ? (prog_cap * 2) : PROGRAM_INIT_CAP;
			char* np = (char*)realloc(program, new_cap);
			if (!np) { perror("realloc"); break; }
			program = np;
			prog_cap = new_cap;
		}
		program[prog_len++] = (char)ch;

		// 連續執行，直到消化到最新指令（或遇到需等待配對括號）
		bool need_more_code = false;
		while (ip < prog_len) {
			char op = program[ip];
			switch (op) {
				case '>':
					// 固定視窗模式：限制指標在 0..VIEW_WIDTH-1
					if (ptr + 1 < VIEW_WIDTH) ptr++;
					ip++;
					break;
				case '<':
					// 固定視窗模式：限制指標在 0..VIEW_WIDTH-1
					if (ptr > 0) ptr--;
					ip++;
					break;
				case '+':
					tape[ptr]++;
					ip++;
					break;
				case '-':
					tape[ptr]--;
					ip++;
					break;
				case '.': {
					unsigned char c = tape[ptr];
					append_char(&out_buf, &out_len, &out_cap, (char)c);
					ip++;
				} break;
				case ',': {
					// 讀取下一個按鍵作為輸入資料
					unsigned char inb = read_input_byte();
					tape[ptr] = inb;
					ip++;
				} break;
				case '[': {
					if (tape[ptr] == 0) {
						ssize_t match = find_matching_forward(program, prog_len, ip);
						if (match < 0) {
							// 尚未有對應的 ']'，等待更多輸入
							need_more_code = true;
						} else {
							ip = (size_t)match + 1;
						}
					} else {
						ip++;
					}
				} break;
				case ']': {
					ssize_t match = find_matching_backward(program, prog_len, ip);
					if (match < 0) {
						// 不匹配，忽略此指令
						ip++; 
						break;
					}
					if (tape[ptr] != 0) {
						ip = (size_t)match + 1; // 跳回到對應 '[' 之後
					} else {
						ip++;
					}
				} break;
				default:
					ip++;
					break;
			}

			// 每步後刷新畫面
			render_state(tape, ptr, ip, program, prog_len, out_buf, out_len);

			if (need_more_code) break;
		}
	}

	disable_raw_mode();
	putchar('\n');

	free(tape);
	free(program);
	free(out_buf);
	return 0;
}


