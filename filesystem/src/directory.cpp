// directory.cpp
#include "../include/inode.h"
#include <cstring>

// 向目录中添加条目
int dir_add_entry(int fd, Inode* dir_inode, int dir_inode_id, 
                  const char* name, int inode_id) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1;
    }
    
    // 第一步：验证不存在同名条目
    if (dir_find_entry(fd, dir_inode, name) != -1) {
        return -1;
    }
    
    // 创建新目录项
    DirEntry new_entry;
    new_entry.inode_id = inode_id;
    strncpy(new_entry.name, name, DIR_NAME_SIZE - 1);
    new_entry.name[DIR_NAME_SIZE - 1] = '\0';
    
    // 第二步：写入目录项（原子操作，inode_write_data已支持原子性）
    int entry_index = dir_inode->size / sizeof(DirEntry);
    int offset = entry_index * sizeof(DirEntry);
    
    int result = inode_write_data(fd, dir_inode, dir_inode_id, 
                                  (const char*)&new_entry, 
                                  offset, sizeof(DirEntry));
    
    return (result == sizeof(DirEntry)) ? 0 : -1;
}

// 在目录中查找条目
int dir_find_entry(int fd, const Inode* dir_inode, const char* name) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    
    for (int i = 0; i < entry_count; i++) {
        DirEntry entry;
        int offset = i * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&entry, offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            continue;
        }
        
        if (strcmp(entry.name, name) == 0) {
            return entry.inode_id; // 返回找到的inode id
        }
    }
    
    return -1; // 未找到
}

// 获取目录中的第index个条目
int dir_get_entry(int fd, const Inode* dir_inode, int index, DirEntry* entry) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    if (index >= entry_count) {
        return -1; // 索引超出范围
    }
    
    int offset = index * sizeof(DirEntry);
    int bytes_read = inode_read_data(fd, dir_inode, (char*)entry, offset, sizeof(DirEntry));
    
    return (bytes_read == sizeof(DirEntry)) ? 0 : -1;
}

// 从目录中删除条目
int dir_remove_entry(int fd, Inode* dir_inode, int dir_inode_id, const char* name) {
    if (dir_inode->type != INODE_TYPE_DIR) {
        return -1; // 不是目录
    }
    
    int entry_count = dir_inode->size / sizeof(DirEntry);
    int found_index = -1;
    
    // 查找要删除的条目
    for (int i = 0; i < entry_count; i++) {
        DirEntry entry;
        int offset = i * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&entry, offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            continue;
        }
        
        if (strcmp(entry.name, name) == 0) {
            found_index = i;
            break;
        }
    }
    
    if (found_index == -1) {
        return -1; // 条目不存在
    }
    
    // 如果不是最后一个条目，需要用最后一个条目覆盖它
    if (found_index < entry_count - 1) {
        DirEntry last_entry;
        int last_offset = (entry_count - 1) * sizeof(DirEntry);
        
        if (inode_read_data(fd, dir_inode, (char*)&last_entry, last_offset, sizeof(DirEntry)) 
            != sizeof(DirEntry)) {
            return -1;  // 读取失败
        }
        
        int target_offset = found_index * sizeof(DirEntry);
        int written = inode_write_data(fd, dir_inode, dir_inode_id, (char*)&last_entry, 
                                       target_offset, sizeof(DirEntry));
        if (written != sizeof(DirEntry)) {
            return -1;  // 写入失败，不修改目录大小
        }
    }
    
    // 只有在成功写入后才缩小目录大小
    dir_inode->size -= sizeof(DirEntry);
    
    // 写回inode
    write_inode(fd, dir_inode_id, dir_inode);
    
    return 0;
}