CC = gcc
CFLAGS = -fPIC -Wall -O2
LDFLAGS = -shared

TARGET = heappulse.so
SRC = HeapPulse.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ 
