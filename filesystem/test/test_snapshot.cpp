// test/test_snapshot.cpp
#include "../include/disk.h"
#include "../include/inode.h"
#include <iostream>
#include <cstring>
#include <cassert>
#include <unistd.h>
using namespace std;

// 在 test/test_snapshot.cpp 中修改 test_snapshot_basic 函数
void test_snapshot_basic() {
    cout << "=== 测试快照基本功能 ===" << endl;
    
    // 打开磁盘
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    cout << "磁盘打开成功" << endl;
    
    // 创建快照1
    int snap1_id = create_snapshot(fd, "snapshot1");
    assert(snap1_id >= 0);
    cout << "创建快照1成功，ID: " << snap1_id << endl;
    
    // 创建快照2
    int snap2_id = create_snapshot(fd, "snapshot2");
    assert(snap2_id >= 0);
    cout << "创建快照2成功，ID: " << snap2_id << endl;
    
    // 尝试创建更多快照
    int created_snapshots[10];
    int snapshot_count = 2; // 已经创建了2个快照
    created_snapshots[0] = snap1_id;
    created_snapshots[1] = snap2_id;
    
    for (int i = 2; i < 10; i++) {
        char snap_name[32];
        snprintf(snap_name, sizeof(snap_name), "snapshot%d", i+1);
        int snap_id = create_snapshot(fd, snap_name);
        if (snap_id < 0) {
            cout << "达到快照上限，无法创建更多快照" << endl;
            break;
        }
        created_snapshots[snapshot_count] = snap_id;
        snapshot_count++;
        cout << "创建快照" << (i+1) << "成功，ID: " << snap_id << endl;
    }
    
    // 删除快照1
    int result = delete_snapshot(fd, snap1_id);
    assert(result == 0);
    cout << "删除快照1成功" << endl;
    
    // 尝试再次删除同一个快照（应该失败）
    result = delete_snapshot(fd, snap1_id);
    assert(result == -1);
    cout << "尝试删除已删除的快照失败（预期行为）" << endl;
    
    // 清理所有创建的快照
    for (int i = 1; i < snapshot_count; i++) { // 从索引1开始，因为snap1已经删除了
        delete_snapshot(fd, created_snapshots[i]);
    }
    
    disk_close(fd);
    cout << "快照基本功能测试通过" << endl;
}

void test_snapshot_with_files() {
    cout << "\n=== 测试快照与文件操作 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    
    // 创建一个测试文件
    int file_inode_id = alloc_inode(fd);
    assert(file_inode_id >= 0);
    
    Inode file_inode;
    init_inode(&file_inode, INODE_TYPE_FILE);
    
    // 写入一些数据
    const char* test_data = "这是快照测试的数据内容。";
    int data_len = strlen(test_data);
    
    int written = inode_write_data(fd, &file_inode, file_inode_id, test_data, 0, data_len);
    assert(written == data_len);
    cout << "写入测试数据成功: " << test_data << endl;
    
    // 创建快照
    int snapshot_id = create_snapshot(fd, "file_snapshot");
    assert(snapshot_id >= 0);
    cout << "创建文件快照成功，ID: " << snapshot_id << endl;
    
    // 修改文件内容
    const char* modified_data = "这是修改后的数据内容。";
    int modified_len = strlen(modified_data);
    
    written = inode_write_data(fd, &file_inode, file_inode_id, modified_data, 0, modified_len);
    assert(written == modified_len);
    cout << "修改文件内容成功: " << modified_data << endl;
    
    // 验证文件当前内容
    char read_buffer[1024];
    int read_bytes = inode_read_data(fd, &file_inode, read_buffer, 0, modified_len);
    assert(read_bytes == modified_len);
    read_buffer[read_bytes] = '\0';
    assert(strcmp(modified_data, read_buffer) == 0);
    cout << "验证当前文件内容: " << read_buffer << endl;
    
    // 清理：删除创建的快照和文件
    delete_snapshot(fd, snapshot_id);
    // 注意：这里我们没有释放inode，因为在实际文件系统中不会这样做
    
    disk_close(fd);
    cout << "快照与文件操作测试通过" << endl;
}

void test_cow_mechanism() {
    cout << "\n=== 测试COW机制 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    
    // 分配一个数据块
    int block_id = alloc_block(fd);
    assert(block_id >= 0);
    cout << "分配数据块成功，ID: " << block_id << endl;
    
    // 检查引用计数
    int ref_count = get_block_ref_count(fd, block_id);
    cout << "初始引用计数: " << ref_count << endl;
    assert(ref_count == 1);  // 应该是1，因为我们刚分配了这个块
    
    // 增加引用计数
    int result = increment_block_ref_count(fd, block_id);
    assert(result == 0);
    
    ref_count = get_block_ref_count(fd, block_id);
    cout << "增加引用计数后: " << ref_count << endl;
    assert(ref_count == 2);
    
    // 执行COW操作（应该会复制块）
    int new_block_id = copy_on_write_block(fd, block_id);
    assert(new_block_id != block_id);
    cout << "COW复制块成功，新块ID: " << new_block_id << endl;
    
    // 检查原块引用计数
    ref_count = get_block_ref_count(fd, block_id);
    cout << "原块引用计数: " << ref_count << endl;
    assert(ref_count == 1);
    
    // 检查新块引用计数
    ref_count = get_block_ref_count(fd, new_block_id);
    cout << "新块引用计数: " << ref_count << endl;
    assert(ref_count == 1);  // 新块的引用计数应该是1，因为alloc_block初始化为1
    
    // 释放块
    free_block(fd, block_id);
    free_block(fd, new_block_id);
    cout << "释放块成功" << endl;
    
    disk_close(fd);
    cout << "COW机制测试通过" << endl;
}

// 在 test/test_snapshot.cpp 中修改 test_multiple_snapshots 函数
void test_multiple_snapshots() {
    cout << "\n=== 测试多重快照 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    
    // 创建多个快照
    int snapshot_ids[5];
    int created_count = 0;
    for (int i = 0; i < 5; i++) {
        char snap_name[32];
        snprintf(snap_name, sizeof(snap_name), "multi_snap_%d", i);
        snapshot_ids[i] = create_snapshot(fd, snap_name);
        if (snapshot_ids[i] < 0) {
            cout << "达到快照上限，创建了 " << i << " 个快照" << endl;
            break;
        }
        created_count++;
        cout << "创建快照 " << i << " 成功，ID: " << snapshot_ids[i] << endl;
    }
    
    // 随机删除一些快照
    for (int i = 0; i < 3 && i < created_count; i++) {
        if (snapshot_ids[i] >= 0) {
            int result = delete_snapshot(fd, snapshot_ids[i]);
            assert(result == 0);
            cout << "删除快照ID " << snapshot_ids[i] << " 成功" << endl;
        }
    }
    
    // 创建更多快照（利用已释放的槽位）
    for (int i = 0; i < 3; i++) {
        char snap_name[32];
        snprintf(snap_name, sizeof(snap_name), "reuse_snap_%d", i);
        int snap_id = create_snapshot(fd, snap_name);
        if (snap_id < 0) {
            cout << "无法创建更多快照" << endl;
            break;
        }
        cout << "复用快照槽位创建快照成功，ID: " << snap_id << endl;
    }
    
    // 清理所有创建的快照
    // 注意：由于一些快照已经被删除，我们需要重新列出所有快照并删除它们
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(fd, snapshots, MAX_SNAPSHOTS);
    for (int i = 0; i < count; i++) {
        if (strncmp(snapshots[i].name, "multi_snap_", 11) == 0 || 
            strncmp(snapshots[i].name, "reuse_snap_", 11) == 0) {
            delete_snapshot(fd, snapshots[i].id);
        }
    }
    
    disk_close(fd);
    cout << "多重快照测试通过" << endl;
}

// 在 test/test_snapshot.cpp 中添加新的测试函数
void test_list_snapshots() {
    cout << "\n=== 测试列出快照功能 ===" << endl;
    
    int fd = disk_open("../disk/disk.img");
    assert(fd >= 0);
    
    // 创建几个快照用于测试
    int snap1_id = create_snapshot(fd, "list_test_1");
    assert(snap1_id >= 0);
    
    int snap2_id = create_snapshot(fd, "list_test_2");
    assert(snap2_id >= 0);
    
    // 列出快照
    Snapshot snapshots[MAX_SNAPSHOTS];
    int count = list_snapshots(fd, snapshots, MAX_SNAPSHOTS);
    assert(count >= 2);
    
    cout << "找到 " << count << " 个快照" << endl;
    
    bool found_snap1 = false, found_snap2 = false;
    for (int i = 0; i < count; i++) {
        if (snapshots[i].id == snap1_id && strcmp(snapshots[i].name, "list_test_1") == 0) {
            found_snap1 = true;
        }
        if (snapshots[i].id == snap2_id && strcmp(snapshots[i].name, "list_test_2") == 0) {
            found_snap2 = true;
        }
    }
    
    assert(found_snap1);
    assert(found_snap2);
    
    cout << "成功列出并验证快照信息" << endl;
    
    // 清理：删除创建的快照
    delete_snapshot(fd, snap1_id);
    delete_snapshot(fd, snap2_id);
    
    disk_close(fd);
    cout << "列出快照功能测试通过" << endl;
}

// 修改 test/test_snapshot.cpp 中的 main 函数
int main() {
    cout << "快照功能测试开始..." << endl;
    
    try {
        test_snapshot_basic();
        test_snapshot_with_files();
        test_cow_mechanism();
        test_multiple_snapshots();
        test_list_snapshots();  // 添加这一行
        
        cout << "\n=== 所有快照测试通过! ===" << endl;
    } catch (const exception& e) {
        cout << "测试失败: " << e.what() << endl;
        return 1;
    }
    
    return 0;
}