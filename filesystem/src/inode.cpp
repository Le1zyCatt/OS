// inode.cpp
#include "../include/inode.h"
#include <cstring>

// 计算每个块能存放多少个块指针
const int POINTERS_PER_BLOCK = BLOCK_SIZE / sizeof(int);

// 初始化inode
void init_inode(Inode* inode, int type) {
    inode->type = type;
    inode->size = 0;
    inode->block_count = 0;
    
    // 初始化直接块指针
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = -1;
    }
    
    // 初始化间接块指针
    inode->indirect_block = -1;
}

// 将inode写入磁盘
int write_inode(int fd, int inode_id, const Inode* inode) {
    // 计算inode在inode表中的位置
    int inode_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_id = INODE_TABLE_START + (inode_id / inode_per_block);
    int offset = inode_id % inode_per_block;
    
    // 读取对应的块
    char buf[BLOCK_SIZE];
    read_block(fd, block_id, buf);
    
    // 更新inode
    Inode* inodes = (Inode*)buf;
    inodes[offset] = *inode;
    
    // 写回磁盘
    write_block(fd, block_id, buf);
    
    return 0;
}

// 从磁盘读取inode
int read_inode(int fd, int inode_id, Inode* inode) {
    // 计算inode在inode表中的位置
    int inode_per_block = BLOCK_SIZE / sizeof(Inode);
    int block_id = INODE_TABLE_START + (inode_id / inode_per_block);
    int offset = inode_id % inode_per_block;
    
    // 读取对应的块
    char buf[BLOCK_SIZE];
    read_block(fd, block_id, buf);
    
    // 获取inode
    Inode* inodes = (Inode*)buf;
    *inode = inodes[offset];
    
    return 0;
}

// 为inode分配一个数据块
int inode_alloc_block(int fd, Inode* inode) {
    // 分配一个数据块
    int block_id = alloc_block(fd);
    if (block_id == -1) {
        return -1; // 没有可用的数据块
    }
    
    // 如果直接块还有空间
    if (inode->block_count < DIRECT_BLOCK_COUNT) {
        inode->direct_blocks[inode->block_count] = block_id;
    } else {
        // 需要使用间接块
        // 如果还没有间接块，则分配一个
        if (inode->indirect_block == -1) {
            inode->indirect_block = alloc_block(fd);
            if (inode->indirect_block == -1) {
                // 回滚之前分配的数据块
                free_block(fd, block_id);
                return -1;
            }
            
            // 初始化间接块
            int pointers[POINTERS_PER_BLOCK];
            for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
                pointers[i] = -1;
            }
            pointers[0] = block_id;
            write_block(fd, inode->indirect_block, pointers);
        } else {
            // 读取现有的间接块
            int pointers[POINTERS_PER_BLOCK];
            read_block(fd, inode->indirect_block, pointers);
            
            // 在间接块中找到空闲位置
            int indirect_index = inode->block_count - DIRECT_BLOCK_COUNT;
            pointers[indirect_index] = block_id;
            
            // 写回间接块
            write_block(fd, inode->indirect_block, pointers);
        }
    }
    
    inode->block_count++;
    inode->size += BLOCK_SIZE; // 简化处理，实际应根据写入数据量调整
    
    return block_id;
}

// 释放inode占用的所有数据块
void inode_free_blocks(int fd, Inode* inode) {
    // 释放直接块
    for (int i = 0; i < inode->block_count && i < DIRECT_BLOCK_COUNT; i++) {
        if (inode->direct_blocks[i] != -1) {
            free_block(fd, inode->direct_blocks[i]);
        }
    }
    
    // 释放间接块指向的数据块
    if (inode->indirect_block != -1) {
        int pointers[POINTERS_PER_BLOCK];
        read_block(fd, inode->indirect_block, pointers);
        
        int indirect_count = inode->block_count - DIRECT_BLOCK_COUNT;
        for (int i = 0; i < indirect_count && i < POINTERS_PER_BLOCK; i++) {
            if (pointers[i] != -1) {
                free_block(fd, pointers[i]);
            }
        }
        
        // 释放间接块本身
        free_block(fd, inode->indirect_block);
    }
    
    // 重置inode
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = -1;
    }
    inode->indirect_block = -1;
    inode->block_count = 0;
    inode->size = 0;
}