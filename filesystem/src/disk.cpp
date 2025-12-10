// disk.cpp
#include "../include/disk.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>

int disk_open(const char* path) {
    int fd = open(path, O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        perror("open disk");
        exit(1);
    }
    return fd;
}

void disk_close(int fd) {
    close(fd);
}

void read_block(int fd, int block_id, void* buf) {
    off_t offset = (off_t)block_id * BLOCK_SIZE;
    lseek(fd, offset, SEEK_SET);
    read(fd, buf, BLOCK_SIZE);
}

void write_block(int fd, int block_id, const void* buf) {
    off_t offset = (off_t)block_id * BLOCK_SIZE;
    lseek(fd, offset, SEEK_SET);
    write(fd, buf, BLOCK_SIZE);
}
