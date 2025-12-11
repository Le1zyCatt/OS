CXX = g++
CXXFLAGS = -std=c++17 -Wall -I../include

BIN_DIR = ../bin
DISK_DIR = ../disk

TARGET_MKFS = $(BIN_DIR)/mkfs

SRC = disk.cpp ../scripts/mkfs.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET_MKFS)

$(TARGET_MKFS): $(OBJ)
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

../scripts/%.o: ../scripts/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o ../scripts/*.o
	rm -f $(TARGET_MKFS)

.PHONY: all clean
