CC = gcc
CLANG_FORMAT = clang-format
CFLAGS = -O2 -Wall
OBJS = daemon.o main.o
CSRCS = $(OBJS:.o=.c)
HEADERS = $(wildcard *.h)
TARGET = server

.PHONY: all clean format
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

format: $(CSRCS) $(HEADERS)
	$(CLANG_FORMAT) -i $^
