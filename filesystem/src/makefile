# src/Makefile
CXX = g++
CXXFLAGS = -Wall -std=c++11
INCLUDES = -I../include
TARGET = ../bin/mkfs
SOURCES = ../scripts/mkfs.cpp disk.cpp

all: $(TARGET)

$(TARGET): $(SOURCES)
	@mkdir -p ../bin
	$(CXX) $(CXXFLAGS) $(INCLUDES) $^ -o $@

clean:
	rm -f ../bin/mkfs

.PHONY: all clean