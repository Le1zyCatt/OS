// mkfs.cpp
#include "../include/disk.h"
#include <unistd.h>
#include <iostream>
#include <cstring>
using namespace std;

int main() {
    int fd = disk_open("../disk/disk.img");

    // 扩展文件到 DISK_SIZE
    lseek(fd, DISK_SIZE - 1, SEEK_SET);
    write(fd, "\0", 1);

    // ---- Superblock ----
    Superblock sb{};
    sb.block_size = BLOCK_SIZE;
    sb.block_count = BLOCK_COUNT;
    sb.inode_count = 128;
    sb.free_inode_count = 128;
    sb.free_block_count = BLOCK_COUNT - DATA_BLOCK_START;

    char buf[BLOCK_SIZE];

    memset(buf, 0, BLOCK_SIZE);
    memcpy(buf, &sb, sizeof(sb));
    write_block(fd, SUPERBLOCK_BLOCK, buf);

    // ---- inode bitmap ----
    memset(buf, 0, BLOCK_SIZE);
    write_block(fd, INODE_BITMAP_BLOCK, buf);

    // ---- block bitmap ----
    memset(buf, 0, BLOCK_SIZE);

    // 前面的 block 已经被占用，需要标记为 1
    for (int i = 0; i < DATA_BLOCK_START; i++) {
        buf[i / 8] |= (1 << (i % 8));
    }
    write_block(fd, BLOCK_BITMAP_BLOCK, buf);

    // ---- inode table (16 blocks) ----
    memset(buf, 0, BLOCK_SIZE);
    for (int i = 0; i < INODE_TABLE_BLOCK_COUNT; i++) {
        write_block(fd, INODE_TABLE_START + i, buf);
    }

    cout << "disk.img 格式化完成！" << endl;

    disk_close(fd);
    return 0;
}
