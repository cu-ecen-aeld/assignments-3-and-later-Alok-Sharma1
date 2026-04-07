# Define the compiler. If CROSS_COMPILE is not specified, it defaults to native 'gcc'.
CC = $(CROSS_COMPILE)gcc

# Compiler flags: show all warnings, treat warnings as errors, and include debug info
CFLAGS = -Wall -Werror -g

# Target executable name
TARGET = writer

# Source files and object files
SRC = writer.c
OBJ = $(SRC:.c=.o)

# Default target
all: $(TARGET)

# Build the target executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean target to remove the application and all .o files
clean:
	rm -f $(TARGET) $(OBJ)