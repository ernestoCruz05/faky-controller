CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pedantic -D_XOPEN_SOURCE=500
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

install-udev:
	sudo cp 99-xbox-controller.rules /etc/udev/rules.d/
	sudo udevadm control --reload-rules
	sudo udevadm trigger

run: $(TARGET)
	sudo ./$(TARGET)

.PHONY: all clean install-udev run
