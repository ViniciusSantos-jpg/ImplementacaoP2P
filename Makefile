# Nome do compilador
CC = gcc

# Flags de compilação
CFLAGS = -Wall -pthread

# Nome do executável
TARGET = rca

# Arquivo-fonte
SRC = rca.c

# Regra padrão
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

# Limpa arquivos compilados
clean:
	rm -f $(TARGET)
