// disk.h
#ifndef FS_DISK_H
#define FS_DISK_H

#include <cstdint>

const int BLOCK_SIZE = 1024;               // 1KB block
const int DISK_SIZE  = 100 * 1024 * 1024;   // 100MB
const int BLOCK_COUNT = DISK_SIZE / BLOCK_SIZE;

// layout
const int SUPERBLOCK_BLOCK = 0;
const int INODE_BITMAP_BLOCK = 1;
const int BLOCK_BITMAP_BLOCK = 2;
const int INODE_TABLE_START = 3;
const int INODE_TABLE_BLOCK_COUNT = 16;

// 快照相关常量
const int SNAPSHOT_TABLE_START = INODE_TABLE_START + INODE_TABLE_BLOCK_COUNT;
const int SNAPSHOT_TABLE_BLOCKS = 4;  // 4个块用于快照表

// 数据块区域相应调整
const int DATA_BLOCK_START = SNAPSHOT_TABLE_START + SNAPSHOT_TABLE_BLOCKS;

// 快照结构体
struct Snapshot {
    int id;              // 快照ID
    int active;          // 是否激活 (1=active, 0=inactive)
    int timestamp;       // 时间戳
    int root_inode_id;   // 快照时的根inode ID
    char name[32];       // 快照名称
};

// 现在可以安全地定义MAX_SNAPSHOTS
const int MAX_SNAPSHOTS = (SNAPSHOT_TABLE_BLOCKS * BLOCK_SIZE) / sizeof(Snapshot);

struct Superblock {
    int block_size;
    int block_count;
    int inode_count;
    int free_inode_count;
    int free_block_count;
};

// 添加到disk.h的结构体定义部分

// 扩展的块位图项，包含引用计数
struct BlockBitmapEntry {
    unsigned char allocated : 1;  // 是否已分配
    unsigned char reserved : 7;   // 保留位
    unsigned char ref_count;      // 引用计数 (最多255个引用)
};

// 快照操作函数声明
int create_snapshot(int fd, const char* name);
int restore_snapshot(int fd, int snapshot_id);
int delete_snapshot(int fd, int snapshot_id);
int list_snapshots(int fd);

// Bitmap 操作函数声明
int alloc_inode(int fd);
void free_inode(int fd, int inode_id);
int alloc_block(int fd);
void free_block(int fd, int block_id);

int disk_open(const char* path);
void disk_close(int fd);

void read_block(int fd, int block_id, void* buf);
void write_block(int fd, int block_id, const void* buf);

// 新增数据块操作函数声明
int read_data_block(int fd, int block_id, void* buf, int offset, int size);
int write_data_block(int fd, int block_id, const void* data, int offset, int size);

// 新增superblock操作函数声明
void read_superblock(int fd, Superblock* sb);
void write_superblock(int fd, const Superblock* sb);

// 添加新的函数声明
int increment_block_ref_count(int fd, int block_id);
int decrement_block_ref_count(int fd, int block_id);
int get_block_ref_count(int fd, int block_id);
int copy_on_write_block(int fd, int block_id);

#endif