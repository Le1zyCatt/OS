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
    // 注意：这里不直接增加size，而是在实际写入数据时增加
    
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

// 根据逻辑块号获取物理块号
// int get_block_id(const Inode* inode, int logical_block_num) {
//     if (logical_block_num < 0) {
//         return -1;
//     }
    
//     // 直接块
//     if (logical_block_num < DIRECT_BLOCK_COUNT) {
//         return inode->direct_blocks[logical_block_num];
//     }
    
//     // 间接块
//     if (inode->indirect_block != -1) {
//         int indirect_index = logical_block_num - DIRECT_BLOCK_COUNT;
//         if (indirect_index < POINTERS_PER_BLOCK) {
//             int pointers[POINTERS_PER_BLOCK];
//             // 这里我们需要磁盘文件描述符来读取间接块
//             // 由于此函数没有fd参数，我们只能返回-1
//             // 实际使用时应在inode_read_data/inode_write_data中处理
//             return -1;
//         }
//     }
    
//     return -1; // 无法找到对应的物理块
// }

// 向inode写入数据
int inode_write_data(int fd, Inode* inode, int inode_id, const char* data, int offset, int size) {
    if (size <= 0) return 0;
    
    int bytes_written = 0;
    int current_offset = offset;
    int remaining = size;
    
    while (remaining > 0) {
        // 计算当前数据应该写入的逻辑块号和块内偏移
        int logical_block_num = current_offset / BLOCK_SIZE;
        int block_offset = current_offset % BLOCK_SIZE;
        int chunk_size = BLOCK_SIZE - block_offset;
        if (chunk_size > remaining) {
            chunk_size = remaining;
        }
        
        // 确保有足够的块
        while (logical_block_num >= inode->block_count) {
            if (inode_alloc_block(fd, inode) == -1) {
                // 分配失败，返回已写入的字节数
                write_inode(fd, inode_id, inode);
                return bytes_written;
            }
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
        
        // 写入数据到块中
        write_data_block(fd, physical_block_id, data + bytes_written, block_offset, chunk_size);
        
        bytes_written += chunk_size;
        current_offset += chunk_size;
        remaining -= chunk_size;
    }
    
    // 更新文件大小
    if (offset + size > inode->size) {
        inode->size = offset + size;
    }
    
    // 写回inode
    write_inode(fd, inode_id, inode);
    
    return bytes_written;
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