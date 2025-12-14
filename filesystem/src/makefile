CXX = g++
CXXFLAGS = -std=c++17 -Wall -I../include

BIN_DIR = ../bin
DISK_DIR = ../disk
TEST_DIR = ../test

TARGET_MKFS = $(BIN_DIR)/mkfs
TARGET_TEST = $(BIN_DIR)/test_filesystem

SRC = disk.cpp inode.cpp directory.cpp path.cpp  # 确保包含了 path.cpp
OBJ = $(SRC:.cpp=.o)

all: $(TARGET_MKFS) $(TARGET_TEST)

$(TARGET_MKFS): $(OBJ) ../scripts/mkfs.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TARGET_TEST): $(OBJ) $(TEST_DIR)/test_filesystem.o
	mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

../scripts/%.o: ../scripts/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_DIR)/%.o: $(TEST_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o ../scripts/*.o $(TEST_DIR)/*.o
	rm -f $(TARGET_MKFS) $(TARGET_TEST)

.PHONY: all clean