# Makefile - File Monitor (C standard + inotify Linux)
# Pas de bibliothèques tierces

CC = gcc
CFLAGS = -Wall -Wextra -Werror -O2 -std=c99 -g
LDFLAGS =
TARGET = file_monitor

SRCS = main.c config.c logger.c file_monitor.c auditd.c
OBJS = $(SRCS:.c=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ ../42/libft/libft.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

# Test avec surveillance locale (sans root)
test-local: $(TARGET)
	@echo "Creation config locale..."
	@echo "watch:" > config_local.yaml
	@echo "  - ." >> config_local.yaml
	@echo "exclude:" >> config_local.yaml
	@echo "  - .git" >> config_local.yaml
	@echo "log_file: monitor_local.log" >> config_local.yaml
	@echo "recursive: true" >> config_local.yaml
	./$(TARGET) config_local.yaml
