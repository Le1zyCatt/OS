// 在 disk.cpp 中添加以下实现
#include "../include/disk.h"
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <cassert>

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

// 读取数据块的一部分内容
int read_data_block(int fd, int block_id, void* buf, int offset, int size) {
    // 参数检查
    if (offset < 0 || size <= 0 || offset + size > BLOCK_SIZE) {
        return -1;
    }
    
    // 读取整个块
    char block_buf[BLOCK_SIZE];
    read_block(fd, block_id, block_buf);
    
    // 拷贝需要的数据
    memcpy(buf, block_buf + offset, size);
    
    return size;
}

// 写入数据块的一部分内容
int write_data_block(int fd, int block_id, const void* data, int offset, int size) {
    // 参数检查
    if (offset < 0 || size <= 0 || offset + size > BLOCK_SIZE) {
        return -1;
    }
    
    // 读取整个块
    char block_buf[BLOCK_SIZE];
    read_block(fd, block_id, block_buf);
    
    // 更新数据
    memcpy(block_buf + offset, data, size);
    
    // 写回整个块
    write_block(fd, block_id, block_buf);
    
    return size;
}

// superblock相关逻辑
// superblock读取
void read_superblock(int fd, Superblock* sb) {
    char buf[BLOCK_SIZE];
    read_block(fd, SUPERBLOCK_BLOCK, buf);
    memcpy(sb, buf, sizeof(Superblock));
}

// superblock写回
void write_superblock(int fd, const Superblock* sb) {
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, sb, sizeof(Superblock));
    write_block(fd, SUPERBLOCK_BLOCK, buf);
}

// 分配一个 inode
int alloc_inode(int fd) {
    char buf[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, buf);
    
    // 查找第一个为0的位（空闲inode）
    for (int i = 0; i < BLOCK_SIZE * 8; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        if (!(buf[byte_index] & (1 << bit_index))) {
            // 找到空闲 inode，标记为已使用
            buf[byte_index] |= (1 << bit_index);
            write_block(fd, INODE_BITMAP_BLOCK, buf);
            
            // 更新superblock中的空闲inode计数
            Superblock sb;
            read_superblock(fd, &sb);
            sb.free_inode_count--;
            write_superblock(fd, &sb);
            
            return i;
        }
    }
    
    // 没有可用的 inode
    return -1;
}

// 释放一个 inode
void free_inode(int fd, int inode_id) {
    char buf[BLOCK_SIZE];
    read_block(fd, INODE_BITMAP_BLOCK, buf);
    
    int byte_index = inode_id / 8;
    int bit_index = inode_id % 8;
    
    // 标记为未使用
    buf[byte_index] &= ~(1 << bit_index);
    write_block(fd, INODE_BITMAP_BLOCK, buf);
    
    // 更新superblock中的空闲inode计数
    Superblock sb;
    read_superblock(fd, &sb);
    sb.free_inode_count++;
    write_superblock(fd, &sb);
}

// 分配一个数据块
int alloc_block(int fd) {
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    // 查找第一个为0的位（空闲数据块）
    for (int i = 0; i < BLOCK_COUNT; i++) {
        int byte_index = i / 8;
        int bit_index = i % 8;
        
        if (!(buf[byte_index] & (1 << bit_index))) {
            // 找到空闲数据块，标记为已使用
            buf[byte_index] |= (1 << bit_index);
            write_block(fd, BLOCK_BITMAP_BLOCK, buf);
            
            // 更新superblock中的空闲块计数
            Superblock sb;
            read_superblock(fd, &sb);
            sb.free_block_count--;
            write_superblock(fd, &sb);
            
            return i;
        }
    }
    
    // 没有可用的数据块
    return -1;
}

// 释放一个数据块
void free_block(int fd, int block_id) {
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    
    // 标记为未使用
    buf[byte_index] &= ~(1 << bit_index);
    write_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    // 更新superblock中的空闲块计数
    Superblock sb;
    read_superblock(fd, &sb);
    sb.free_block_count++;
    write_superblock(fd, &sb);
}