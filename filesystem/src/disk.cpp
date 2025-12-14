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

// 修改alloc_block函数
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
            
            // 初始化引用计数为1
            BlockBitmapEntry* entries = (BlockBitmapEntry*)buf;
            entries[i].ref_count = 1;
            
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

// 修改free_block函数
void free_block(int fd, int block_id) {
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    BlockBitmapEntry* entries = (BlockBitmapEntry*)buf;
    
    // 减少引用计数
    if (entries[block_id].ref_count > 1) {
        entries[block_id].ref_count--;
        write_block(fd, BLOCK_BITMAP_BLOCK, buf);
        return;
    }
    
    // 如果引用计数为1或0，则真正释放块
    int byte_index = block_id / 8;
    int bit_index = block_id % 8;
    
    // 标记为未使用
    buf[byte_index] &= ~(1 << bit_index);
    entries[block_id].ref_count = 0;
    
    write_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    // 更新superblock中的空闲块计数
    Superblock sb;
    read_superblock(fd, &sb);
    sb.free_block_count++;
    write_superblock(fd, &sb);
}

// 创建快照
// 将disk.cpp中的create_snapshot函数中的可变长度数组改为固定大小数组
// 修改这一段代码:

// 创建快照
int create_snapshot(int fd, const char* name) {
    // 查找空闲的快照槽位
    // 使用固定大小数组替代可变长度数组
    char buf[BLOCK_SIZE];
    
    // 读取快照表
    int snapshots_per_block = BLOCK_SIZE / sizeof(Snapshot);
    int snapshot_count = 0;
    int free_slot = -1;
    
    // 逐个读取快照表块并查找空闲槽位
    for (int i = 0; i < SNAPSHOT_TABLE_BLOCKS; i++) {
        read_block(fd, SNAPSHOT_TABLE_START + i, buf);
        Snapshot* block_snapshots = (Snapshot*)buf;
        
        for (int j = 0; j < snapshots_per_block && (i * snapshots_per_block + j) < MAX_SNAPSHOTS; j++) {
            int idx = i * snapshots_per_block + j;
            if (!block_snapshots[j].active && free_slot == -1) {
                free_slot = idx;
            }
            snapshot_count++;
        }
        
        // 如果找到了空闲槽位，就不需要继续查找
        if (free_slot != -1) {
            break;
        }
    }
    
    // 如果没有空闲槽位
    if (free_slot == -1) {
        return -1; // 快照数量已达上限
    }
    
    // 创建新快照
    Snapshot new_snapshot;
    new_snapshot.id = free_slot;
    new_snapshot.active = 1;
    new_snapshot.timestamp = (int)time(nullptr);
    new_snapshot.root_inode_id = 0; // 当前根目录inode ID
    strncpy(new_snapshot.name, name, sizeof(new_snapshot.name) - 1);
    new_snapshot.name[sizeof(new_snapshot.name) - 1] = '\0';
    
    // 写入快照到对应的位置
    int block_id = SNAPSHOT_TABLE_START + (free_slot / snapshots_per_block);
    int offset = free_slot % snapshots_per_block;
    
    read_block(fd, block_id, buf);
    Snapshot* snapshots = (Snapshot*)buf;
    snapshots[offset] = new_snapshot;
    write_block(fd, block_id, buf);
    
    return free_slot; // 返回快照ID
}

// 列出所有快照 (这个函数最好移到测试程序或工具程序中)
int list_snapshots(int fd) {
    // 这个函数更适合放在测试程序中，而不是文件系统核心代码中
    return 0;
}

// 删除快照
int delete_snapshot(int fd, int snapshot_id) {
    if (snapshot_id < 0 || snapshot_id >= MAX_SNAPSHOTS) {
        return -1; // 无效的快照ID
    }
    
    char buf[BLOCK_SIZE];
    int block_id = SNAPSHOT_TABLE_START + (snapshot_id * sizeof(Snapshot)) / BLOCK_SIZE;
    int offset = (snapshot_id * sizeof(Snapshot)) % BLOCK_SIZE;
    
    read_block(fd, block_id, buf);
    Snapshot* snapshot = (Snapshot*)(buf + offset);
    
    if (!snapshot->active) {
        return -1; // 快照不存在
    }
    
    // 标记为非活动
    snapshot->active = 0;
    write_block(fd, block_id, buf);
    
    return 0; // 成功删除
}

// 添加到src/disk.cpp末尾

// 增加块引用计数
int increment_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    BlockBitmapEntry* entries = (BlockBitmapEntry*)buf;
    if (entries[block_id].allocated) {
        if (entries[block_id].ref_count < 255) {
            entries[block_id].ref_count++;
            write_block(fd, BLOCK_BITMAP_BLOCK, buf);
            return 0;
        }
    }
    
    return -1;
}

// 减少块引用计数
int decrement_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    BlockBitmapEntry* entries = (BlockBitmapEntry*)buf;
    if (entries[block_id].allocated && entries[block_id].ref_count > 0) {
        entries[block_id].ref_count--;
        write_block(fd, BLOCK_BITMAP_BLOCK, buf);
        return 0;
    }
    
    return -1;
}

// 获取块引用计数
int get_block_ref_count(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    read_block(fd, BLOCK_BITMAP_BLOCK, buf);
    
    BlockBitmapEntry* entries = (BlockBitmapEntry*)buf;
    if (entries[block_id].allocated) {
        return entries[block_id].ref_count;
    }
    
    return -1;
}

// COW复制块
int copy_on_write_block(int fd, int block_id) {
    if (block_id < 0 || block_id >= BLOCK_COUNT) {
        return -1;
    }
    
    // 检查引用计数
    int ref_count = get_block_ref_count(fd, block_id);
    if (ref_count <= 1) {
        // 如果只有一个引用或没有引用，不需要复制
        return block_id;
    }
    
    // 分配新块
    int new_block_id = alloc_block(fd);
    if (new_block_id == -1) {
        return -1;
    }
    
    // 复制数据
    char buf[BLOCK_SIZE];
    read_block(fd, block_id, buf);
    write_block(fd, new_block_id, buf);
    
    // 更新引用计数
    decrement_block_ref_count(fd, block_id);  // 减少旧块引用
    increment_block_ref_count(fd, new_block_id);  // 增加新块引用
    
    return new_block_id;
}

// 添加到src/disk.cpp末尾

// 恢复快照
int restore_snapshot(int fd, int snapshot_id) {
    if (snapshot_id < 0 || snapshot_id >= MAX_SNAPSHOTS) {
        return -1;
    }
    
    char buf[BLOCK_SIZE];
    int block_id = SNAPSHOT_TABLE_START + (snapshot_id * sizeof(Snapshot)) / BLOCK_SIZE;
    int offset = (snapshot_id * sizeof(Snapshot)) % BLOCK_SIZE;
    
    read_block(fd, block_id, buf);
    Snapshot* snapshot = (Snapshot*)(buf + offset);
    
    if (!snapshot->active) {
        return -1; // 快照不存在
    }
    
    // 恢复根inode
    // 注意：在实际实现中，这会涉及更复杂的操作，如递归恢复整个文件系统树
    // 这里只是一个简单的示例
    return 0;
}