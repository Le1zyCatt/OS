#include "../include/disk.h"
#include "../include/inode.h"
#include <iostream>
#include <cstring>
#include <cassert>
using namespace std;

void test_disk_operations() {
    cout << "=== 测试磁盘基本操作 ===" << endl;
    
    // 打开磁盘
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    cout << "磁盘打开成功" << endl;
    
    // 读取superblock
    Superblock sb;
    read_superblock(fd, &sb);
    cout << "初始空闲inode数: " << sb.free_inode_count << endl;
    cout << "初始空闲块数: " << sb.free_block_count << endl;
    
    // 测试块读写
    char write_buf[BLOCK_SIZE];
    char read_buf[BLOCK_SIZE];
    
    memset(write_buf, 0xAA, BLOCK_SIZE); // 填充特定值
    write_block(fd, DATA_BLOCK_START, write_buf);
    
    memset(read_buf, 0, BLOCK_SIZE);
    read_block(fd, DATA_BLOCK_START, read_buf);
    
    assert(memcmp(write_buf, read_buf, BLOCK_SIZE) == 0);
    cout << "块读写测试通过" << endl;
    
    // 测试部分块读写
    char partial_data[] = "Hello FileSystem!";
    write_data_block(fd, DATA_BLOCK_START, partial_data, 100, strlen(partial_data));
    
    char read_partial[100];
    read_data_block(fd, DATA_BLOCK_START, read_partial, 100, strlen(partial_data));
    
    assert(memcmp(partial_data, read_partial, strlen(partial_data)) == 0);
    cout << "部分块读写测试通过" << endl;
    
    disk_close(fd);
}

void test_inode_allocation() {
    cout << "\n=== 测试Inode分配 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 保存初始状态
    Superblock sb_orig;
    read_superblock(fd, &sb_orig);
    
    // 分配几个inode
    int inode_ids[5];
    for (int i = 0; i < 5; i++) {
        inode_ids[i] = alloc_inode(fd);
        assert(inode_ids[i] >= 0);
        cout << "分配inode ID: " << inode_ids[i] << endl;
    }
    
    // 检查superblock更新
    Superblock sb_new;
    read_superblock(fd, &sb_new);
    assert(sb_new.free_inode_count == sb_orig.free_inode_count - 5);
    cout << "Inode计数更新正确" << endl;
    
    // 释放一个inode
    free_inode(fd, inode_ids[0]);
    
    read_superblock(fd, &sb_new);
    assert(sb_new.free_inode_count == sb_orig.free_inode_count - 4);
    cout << "Inode释放和计数更新正确" << endl;
    
    disk_close(fd);
}

void test_block_allocation() {
    cout << "\n=== 测试数据块分配 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 保存初始状态
    Superblock sb_orig;
    read_superblock(fd, &sb_orig);
    
    // 分配几个数据块
    int block_ids[5];
    for (int i = 0; i < 5; i++) {
        block_ids[i] = alloc_block(fd);
        assert(block_ids[i] >= 0);
        cout << "分配数据块 ID: " << block_ids[i] << endl;
    }
    
    // 检查superblock更新
    Superblock sb_new;
    read_superblock(fd, &sb_new);
    assert(sb_new.free_block_count == sb_orig.free_block_count - 5);
    cout << "数据块计数更新正确" << endl;
    
    // 释放一个数据块
    free_block(fd, block_ids[0]);
    
    read_superblock(fd, &sb_new);
    assert(sb_new.free_block_count == sb_orig.free_block_count - 4);
    cout << "数据块释放和计数更新正确" << endl;
    
    disk_close(fd);
}

void test_inode_operations() {
    cout << "\n=== 测试Inode操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 分配一个inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    // 初始化inode
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    cout << "Inode初始化完成" << endl;
    
    // 写入inode
    int result = write_inode(fd, inode_id, &inode);
    assert(result == 0);
    cout << "Inode写入完成" << endl;
    
    // 读取inode
    Inode inode_read;  // 修改变量名避免冲突
    result = read_inode(fd, inode_id, &inode_read);  // 现在可以正确调用函数
    assert(result == 0);
    
    // 验证内容
    assert(inode_read.type == INODE_TYPE_FILE);
    assert(inode_read.size == 0);
    assert(inode_read.block_count == 0);
    cout << "Inode读取和验证完成" << endl;
    
    disk_close(fd);
}

void test_file_data_operations() {
    cout << "\n=== 测试文件数据操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 创建一个文件inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    
    // 写入数据
    const char* test_data = "这是测试数据内容。";
    int data_len = strlen(test_data);
    
    int written = inode_write_data(fd, &inode, inode_id, test_data, 0, data_len);
    assert(written == data_len);
    cout << "写入数据: " << test_data << endl;
    cout << "写入字节数: " << written << endl;
    
    // 读取数据
    char read_buffer[1024];
    int read_bytes = inode_read_data(fd, &inode, read_buffer, 0, data_len);
    assert(read_bytes == data_len);
    
    read_buffer[read_bytes] = '\0'; // 添加字符串结束符
    cout << "读取数据: " << read_buffer << endl;
    cout << "读取字节数: " << read_bytes << endl;
    
    // 验证数据一致性
    assert(strcmp(test_data, read_buffer) == 0);
    cout << "数据一致性验证通过" << endl;
    
    // 测试跨块读写
    char large_data[BLOCK_SIZE * 2];
    memset(large_data, 'A', BLOCK_SIZE);
    memset(large_data + BLOCK_SIZE, 'B', BLOCK_SIZE - 1);
    large_data[BLOCK_SIZE * 2 - 1] = '\0';
    
    // 重新初始化inode
    init_inode(&inode, INODE_TYPE_FILE);
    written = inode_write_data(fd, &inode, inode_id, large_data, 0, BLOCK_SIZE * 2 - 1);
    assert(written == BLOCK_SIZE * 2 - 1);
    cout << "大块数据写入完成" << endl;
    
    char large_read_buffer[BLOCK_SIZE * 2];
    read_bytes = inode_read_data(fd, &inode, large_read_buffer, 0, BLOCK_SIZE * 2 - 1);
    assert(read_bytes == BLOCK_SIZE * 2 - 1);
    
    large_read_buffer[read_bytes] = '\0';
    assert(memcmp(large_data, large_read_buffer, BLOCK_SIZE * 2 - 1) == 0);
    cout << "大块数据读取验证通过" << endl;
    
    disk_close(fd);
}

void test_direct_and_indirect_blocks() {
    cout << "\n=== 测试直接块和间接块 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    
    // 创建一个文件inode
    int inode_id = alloc_inode(fd);
    assert(inode_id >= 0);
    
    Inode inode;
    init_inode(&inode, INODE_TYPE_FILE);
    
    // 写入足够多的数据来触发间接块分配
    // DIRECT_BLOCK_COUNT是10，每个块1024字节
    // 我们写入12个块的数据
    char* large_data = new char[BLOCK_SIZE * 12];
    for (int i = 0; i < BLOCK_SIZE * 12; i++) {
        large_data[i] = 'A' + (i % 26);
    }
    
    int written = inode_write_data(fd, &inode, inode_id, large_data, 0, BLOCK_SIZE * 12);
    assert(written == BLOCK_SIZE * 12);
    cout << "写入12个块的数据完成" << endl;
    
    // 检查inode状态
    cout << "分配的块数: " << inode.block_count << endl;
    cout << "文件大小: " << inode.size << endl;
    assert(inode.block_count == 12);
    assert(inode.size == BLOCK_SIZE * 12);
    
    // 验证直接块指针
    cout << "直接块指针数量: " << DIRECT_BLOCK_COUNT << endl;
    for (int i = 0; i < min(DIRECT_BLOCK_COUNT, inode.block_count); i++) {
        cout << "直接块[" << i << "]: " << inode.direct_blocks[i] << endl;
        assert(inode.direct_blocks[i] != -1);
    }
    
    // 验证间接块指针
    if (inode.block_count > DIRECT_BLOCK_COUNT) {
        cout << "间接块指针: " << inode.indirect_block << endl;
        assert(inode.indirect_block != -1);
    }
    
    // 读取数据验证
    char* read_buffer = new char[BLOCK_SIZE * 12];
    int read_bytes = inode_read_data(fd, &inode, read_buffer, 0, BLOCK_SIZE * 12);
    assert(read_bytes == BLOCK_SIZE * 12);
    
    assert(memcmp(large_data, read_buffer, BLOCK_SIZE * 12) == 0);
    cout << "大文件数据一致性验证通过" << endl;
    
    delete[] large_data;
    delete[] read_buffer;
    
    disk_close(fd);
}

int main() {
    cout << "文件系统测试开始..." << endl;
    
    try {
        test_disk_operations();
        test_inode_allocation();
        test_block_allocation();
        test_inode_operations();
        test_file_data_operations();
        test_direct_and_indirect_blocks();
        
        cout << "\n=== 所有测试通过! ===" << endl;
    } catch (const exception& e) {
        cout << "测试失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}
