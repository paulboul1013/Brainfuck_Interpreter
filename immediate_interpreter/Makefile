CC := gcc
CFLAGS := -O2 -Wall -Wextra -std=c11
LDFLAGS :=

TARGET := bf_immediate
SRC := main.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)


