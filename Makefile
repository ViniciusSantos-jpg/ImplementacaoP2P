CC = gcc

CFLAGS = -Wall -Wextra -g -pthread

SRCS = rca.c

TARGET = rca

all: $(TARGET)

# Regra para construir o executável
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET) *.o log.txt

.PHONY: all clean