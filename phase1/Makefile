CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude

TARGET := MiniOS

SRC := \
	src/main.cpp \
	src/shell.cpp \
	src/commands.cpp \
	src/task.cpp \
	src/scheduler.cpp \
	src/semaphore.cpp \
	src/memory.cpp \
	src/fs.cpp

.PHONY: all run clean rebuild

all:
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

rebuild: clean all