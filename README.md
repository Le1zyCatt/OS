# OS
SCUT操作系统课程大作业：论文审稿系统  
## 1. 目录结构说明
```
.
├── filesystem  
│   ├── bin          # 编译后的二进制文件  
│   │   └── mkfs  
│   ├── disk         # 磁盘文件  
│   │   └── disk.img  
│   ├── include      # 头文件  
│   │   └── disk.h  
│   ├── scripts      # 工具代码  
│   │   └── mkfs.cpp  
│   └── src          # 源代码  
│       ├── disk.cpp  
│       └── makefile  
└── README.md  
```
  
## 2. 设计文档
![os1](https://github.com/user-attachments/assets/7a5e74fc-87c2-4fd6-9b0c-90ed27e5af60)
![os2](https://github.com/user-attachments/assets/1dc7c74e-4302-43fa-b112-cfff4340dbea)

## 3. 相关笔记
### 运算符
| 运算符 | 类型     | 在代码中的应用                                               | 功能说明                                         | 示例代码                                       |
|--------|----------|--------------------------------------------------------------|--------------------------------------------------|------------------------------------------------|
| &      | 按位与   | `buf[byte_index] & (1 << bit_index)`                         | 检查特定位是否为1                                | `if (!(buf[byte_index] & (1 << bit_index)))`   |
| \|     | 按位或   | `buf[i / 8] \|= (1 << (i % 8))`                              | 设置特定位为1                                    | `buf[byte_index] \|= (1 << bit_index)`          |
| &=     | 按位与赋值 | `buf[byte_index] &= ~(1 << bit_index)`                       | 清除特定位（设为0）                              | `buf[byte_index] &= ~(1 << bit_index)`          |
| ~      | 按位取反 | `~(1 << bit_index)`                                          | 创建位掩码，目标位为0，其他位为1                  | `buf[byte_index] &= ~(1 << bit_index)`          |
| <<     | 左移     | `1 << bit_index`                                             | 创建只有目标位为1的掩码                          | `1 << (i % 8)`                                 |
| !      | 逻辑非   | `!(buf[byte_index] & (1 << bit_index))`                      | 取反逻辑值，用于判断位是否为0                    | `if (!(buf[byte_index] & (1 << bit_index)))`   |
| /      | 除法     | `i / 8`                                                      | 计算字节索引                                     | `int byte_index = i / 8`                       |
| %      | 取模     | `i % 8`                                                      | 计算位在字节内的索引                             | `int bit_index = i % 8`                        |
| *      | 乘法     | `BLOCK_SIZE * 8`                                             | 计算总位数                                       | `i < BLOCK_SIZE * 8`                           |
| -      | 减法     | `BLOCK_COUNT - DATA_BLOCK_START`                             | 计算可用数据块数量                               | `sb.free_block_count = BLOCK_COUNT - DATA_BLOCK_START` |
| []     | 下标     | `buf[byte_index]`                                            | 访问数组元素                                     | `buf[byte_index]`                              |
| .      | 成员访问 | `sb.block_size`                                              | 访问结构体成员                                   | `sb.block_size = BLOCK_SIZE`                   |
| sizeof | 大小运算 | `sizeof(sb)`                                                 | 获取数据类型的大小                               | `memcpy(buf, &sb, sizeof(sb))`                 |
| ()     | 函数调用 | `read_block(fd, INODE_BITMAP_BLOCK, buf)`                    | 调用函数                                         | `read_block(fd, INODE_BITMAP_BLOCK, buf)`      |

### 核心位操作模式
1. **位检测**: `buf[index] & (1 << bit_pos)` - 检查某位是否为1
2. **位置1**: `buf[index] |= (1 << bit_pos)` - 将某位设置为1
3. **位清0**: `buf[index] &= ~(1 << bit_pos)` - 将某位设置为0
4. **索引计算**: `index = pos / 8`, `bit_pos = pos % 8` - 将线性位置映射到位图坐标

### 系统调用
| 函数   | 用途         | 在代码中的应用                                   |
|--------|--------------|--------------------------------------------------|
| open   | 打开文件     | `open(path, O_RDWR \| O_CREAT, 0666)`              |
| close  | 关闭文件     | `close(fd)`                                      |
| lseek  | 移动文件指针 | `lseek(fd, offset, SEEK_SET)`                    |
| read   | 读取文件     | `read(fd, buf, BLOCK_SIZE)`                      |
| write  | 写入文件     | `write(fd, buf, BLOCK_SIZE)`                     |

### 文件操作模式
- `O_RDWR`: 读写模式打开文件
- `O_CREAT`: 文件不存在时创建
- `SEEK_SET`: 从文件开头计算偏移量

### 重要常量和宏

| 常量名                 | 值  | 用途说明              |
|------------------------|-----|-----------------------|
| BLOCK_SIZE             | 1024| 磁盘块大小(1KB)       |
| DISK_SIZE              | 10M | 磁盘总大小            |
| SUPERBLOCK_BLOCK       | 0   | 超级块位置            |
| INODE_BITMAP_BLOCK     | 1   | inode位图位置         |
| BLOCK_BITMAP_BLOCK     | 2   | 数据块位图位置        |
| INODE_TABLE_START      | 3   | inode表起始位置       |

