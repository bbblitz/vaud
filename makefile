# the compiler: gcc for C program, define as g++ for C++
CC = g++

# compiler flags:
PREFLAGS  =
POSTFLAGS = -lncurses -ljack

# the build target executable:
TARGET = vaud


all: $(TARGET)

$(TARGET): $(TARGET).c
	$(CC) $(PREFLAGS) -o $(TARGET) $(TARGET).c $(POSTFLAGS)

clean:
	$(RM) $(TARGET)
