CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGET = shad_shell
SRC = shell.c
TEST_TARGET = test_shell
TEST_SRC = test_shell.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_SRC) $(SRC)
	$(CC) $(CFLAGS) -DTEST_BUILD $(TEST_SRC) -o $(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)

.PHONY: all clean test
