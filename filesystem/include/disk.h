// disk.h
#ifndef FS_DISK_H
#define FS_DISK_H

#include <cstdint>

const int BLOCK_SIZE = 1024;               // 1KB block
const int DISK_SIZE  = 10 * 1024 * 1024;   // 10MB
const int BLOCK_COUNT = DISK_SIZE / BLOCK_SIZE;

// layout
const int SUPERBLOCK_BLOCK = 0;
const int INODE_BITMAP_BLOCK = 1;
const int BLOCK_BITMAP_BLOCK = 2;
const int INODE_TABLE_START = 3;
const int INODE_TABLE_BLOCK_COUNT = 16;
const int DATA_BLOCK_START = INODE_TABLE_START + INODE_TABLE_BLOCK_COUNT;

struct Superblock {
    int block_size;
    int block_count;
    int inode_count;
    int free_inode_count;
    int free_block_count;
};

int disk_open(const char* path);
void disk_close(int fd);

void read_block(int fd, int block_id, void* buf);
void write_block(int fd, int block_id, const void* buf);

#endif
