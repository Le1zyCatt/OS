// inode.h
#ifndef FS_INODE_H
#define FS_INODE_H

#include "disk.h"
#include <cstdint>

// inode类型定义
const int INODE_TYPE_FILE = 1;
const int INODE_TYPE_DIR = 2;

// 每个inode中直接块的数量
const int DIRECT_BLOCK_COUNT = 10;

// inode结构定义
struct Inode {
    int type;                           // 文件类型
    int size;                           // 文件大小(字节)
    int block_count;                    // 占用的数据块数量
    int direct_blocks[DIRECT_BLOCK_COUNT]; // 直接数据块指针
    int indirect_block;                 // 一级间接块指针
    // 可以添加更多字段如权限、时间戳等
};

// inode操作函数声明
void init_inode(Inode* inode, int type);
int write_inode(int fd, int inode_id, const Inode* inode);
int read_inode(int fd, int inode_id, Inode* inode);
int inode_alloc_block(int fd, Inode* inode);
void inode_free_blocks(int fd, Inode* inode);

// 新增文件数据操作函数声明
int inode_write_data(int fd, Inode* inode, int inode_id, const char* data, int offset, int size);
int inode_read_data(int fd, const Inode* inode, char* buffer, int offset, int size);

#endif