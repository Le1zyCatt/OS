// inode.cpp
#include "../include/inode.h"
#include <cstring>
#include <vector>
#include <algorithm>
using std::vector;
using std::min;

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
    // 注意：这里不直接增加size，而是在实际写入数据时增加
    
    return block_id;
}

// 释放inode占用的所有数据块
// 修正 inode_free_blocks 函数，确保正确处理引用计数
void inode_free_blocks(int fd, Inode* inode) {
    // 释放直接块
    for (int i = 0; i < inode->block_count && i < DIRECT_BLOCK_COUNT; i++) {
        if (inode->direct_blocks[i] != -1) {
            decrement_block_ref_count(fd, inode->direct_blocks[i]);
            if (get_block_ref_count(fd, inode->direct_blocks[i]) == 0) {
                free_block(fd, inode->direct_blocks[i]);
            }
        }
    }
    
    // 释放间接块指向的数据块
    if (inode->indirect_block != -1) {
        int pointers[POINTERS_PER_BLOCK];
        read_block(fd, inode->indirect_block, pointers);
        
        int indirect_count = inode->block_count - DIRECT_BLOCK_COUNT;
        for (int i = 0; i < indirect_count && i < POINTERS_PER_BLOCK; i++) {
            if (pointers[i] != -1) {
                decrement_block_ref_count(fd, pointers[i]);
                if (get_block_ref_count(fd, pointers[i]) == 0) {
                    free_block(fd, pointers[i]);
                }
            }
        }
        
        // 释放间接块本身
        decrement_block_ref_count(fd, inode->indirect_block);
        if (get_block_ref_count(fd, inode->indirect_block) == 0) {
            free_block(fd, inode->indirect_block);
        }
    }
    
    // 重置inode
    for (int i = 0; i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = -1;
    }
    inode->indirect_block = -1;
    inode->block_count = 0;
    inode->size = 0;
}

// 修改inode_write_data函数以支持COW
// 在 inode.cpp 中修改
int inode_write_data(int fd, Inode* inode, int inode_id, 
                     const char* data, int offset, int size) {
    if (size <= 0) return 0;
    
    // 第一步：计算需要的块数
    int blocks_needed = (offset + size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    
    // 清理旧数据（如果有）
    if (inode->block_count > 0) {
        inode_free_blocks(fd, inode);
    }
    
    // 第二步：分配所有需要的块
    vector<int> new_blocks;
    for (int i = 0; i < blocks_needed; i++) {
        int block_id = alloc_block(fd);
        if (block_id == -1) {
            // 分配失败，回滚所有已分配的块
            for (size_t j = 0; j < new_blocks.size(); j++) {
                free_block(fd, new_blocks[j]);
            }
            return -1;
        }
        new_blocks.push_back(block_id);
    }
    
    // 第三步：写入数据到各个块
    int written = 0;
    for (size_t i = 0; i < new_blocks.size() && written < size; i++) {
        int write_size = (size - written) > BLOCK_SIZE ? 
                        BLOCK_SIZE : (size - written);
        
        // 如果不是首个块，需要零填充
        if (i == 0 && offset > 0) {
            char temp_buf[BLOCK_SIZE];
            memset(temp_buf, 0, BLOCK_SIZE);
            memcpy(temp_buf + offset, (char*)data, write_size);
            write_block(fd, new_blocks[i], temp_buf);
        } else {
            write_block(fd, new_blocks[i], (char*)data + written);
        }
        
        written += write_size;
    }
    
    // 第四步：设置inode块指针
    inode->block_count = blocks_needed;
    
    // 设置直接块指针
    for (int i = 0; i < blocks_needed && i < DIRECT_BLOCK_COUNT; i++) {
        inode->direct_blocks[i] = new_blocks[i];
    }
    
    // 设置间接块指针
    if (blocks_needed > DIRECT_BLOCK_COUNT) {
        // 分配间接块
        inode->indirect_block = alloc_block(fd);
        if (inode->indirect_block == -1) {
            // 分配失败，回滚
            for (size_t j = 0; j < new_blocks.size(); j++) {
                free_block(fd, new_blocks[j]);
            }
            return -1;
        }
        
        // 写入间接块指针表
        int pointers[POINTERS_PER_BLOCK];
        for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
            pointers[i] = -1;
        }
        
        int indirect_count = blocks_needed - DIRECT_BLOCK_COUNT;
        for (int i = 0; i < indirect_count; i++) {
            pointers[i] = new_blocks[DIRECT_BLOCK_COUNT + i];
        }
        
        write_block(fd, inode->indirect_block, (void*)pointers);
    }
    
    // 第五步：更新inode大小和时间戳（原子提交点）
    inode->size = offset + size;
    write_inode(fd, inode_id, inode);
    
    return size;
}

// 从inode读取数据
int inode_read_data(int fd, const Inode* inode, char* buffer, int offset, int size) {
    if (size <= 0 || offset >= inode->size) return 0;
    
    // 限制读取范围不超过文件大小
    if (offset + size > inode->size) {
        size = inode->size - offset;
    }
    
    int bytes_read = 0;
    int current_offset = offset;
    int remaining = size;
    
    while (remaining > 0) {
        // 计算当前数据应该读取的逻辑块号和块内偏移
        int logical_block_num = current_offset / BLOCK_SIZE;
        int block_offset = current_offset % BLOCK_SIZE;
        int chunk_size = BLOCK_SIZE - block_offset;
        if (chunk_size > remaining) {
            chunk_size = remaining;
        }
        
        // 检查是否超出文件范围
        if (logical_block_num >= inode->block_count) {
            break;
        }
        
        // 获取物理块号
        int physical_block_id;
        if (logical_block_num < DIRECT_BLOCK_COUNT) {
            physical_block_id = inode->direct_blocks[logical_block_num];
        } else {
            // 处理间接块
            int indirect_index = logical_block_num - DIRECT_BLOCK_COUNT;
            int pointers[POINTERS_PER_BLOCK];
            read_block(fd, inode->indirect_block, pointers);
            physical_block_id = pointers[indirect_index];
        }
        
        // 从块中读取数据
        read_data_block(fd, physical_block_id, buffer + bytes_read, block_offset, chunk_size);
        
        bytes_read += chunk_size;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }
    
    return bytes_read;
}