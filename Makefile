CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic
LDFLAGS = -lusb-1.0

TARGET = main
SRCS = main.c controller.c
OBJS = $(SRCS:.c=.o)
HEADERS = main.h controller.h

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

run: $(TARGET)
	sudo ./$(TARGET)

.PHONY: all clean install-udev run
