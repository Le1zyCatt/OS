/**
 * platform_compat.h - 跨平台兼容层
 * 
 * 为 Windows 提供 POSIX 风格的文件操作 API
 * 使得 filesystem 模块能够在 Windows 和 Linux 上都能编译运行
 */

#ifndef FS_PLATFORM_COMPAT_H
#define FS_PLATFORM_COMPAT_H

#ifdef _WIN32
    // Windows 平台
    #include <io.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <share.h>
    #include <windows.h>
    
    // Windows 没有 ssize_t，定义一个
    #ifndef ssize_t
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef int ssize_t;
        #endif
    #endif
    
    // Windows 使用 _open, _close, _read, _write, _lseek 等函数
    // 但我们需要原子操作的 pread/pwrite
    
    /**
     * pread - 在指定偏移处读取数据（线程安全）
     * Windows 没有原生 pread，我们使用 overlapped I/O 实现
     */
    static inline ssize_t pread(int fd, void* buf, size_t count, off_t offset) {
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile == INVALID_HANDLE_VALUE) {
            return -1;
        }
        
        OVERLAPPED overlapped = {0};
        overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
        overlapped.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
        
        DWORD bytesRead = 0;
        if (!ReadFile(hFile, buf, (DWORD)count, &bytesRead, &overlapped)) {
            if (GetLastError() == ERROR_HANDLE_EOF) {
                return 0;
            }
            return -1;
        }
        
        return (ssize_t)bytesRead;
    }
    
    /**
     * pwrite - 在指定偏移处写入数据（线程安全）
     * Windows 没有原生 pwrite，我们使用 overlapped I/O 实现
     */
    static inline ssize_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
        HANDLE hFile = (HANDLE)_get_osfhandle(fd);
        if (hFile == INVALID_HANDLE_VALUE) {
            return -1;
        }
        
        OVERLAPPED overlapped = {0};
        overlapped.Offset = (DWORD)(offset & 0xFFFFFFFF);
        overlapped.OffsetHigh = (DWORD)((offset >> 32) & 0xFFFFFFFF);
        
        DWORD bytesWritten = 0;
        if (!WriteFile(hFile, buf, (DWORD)count, &bytesWritten, &overlapped)) {
            return -1;
        }
        
        return (ssize_t)bytesWritten;
    }
    
    // 重定义 POSIX 文件操作宏/函数
    #ifndef O_RDWR
        #define O_RDWR _O_RDWR
    #endif
    #ifndef O_CREAT
        #define O_CREAT _O_CREAT
    #endif
    #ifndef O_TRUNC
        #define O_TRUNC _O_TRUNC
    #endif
    #ifndef O_BINARY
        #define O_BINARY _O_BINARY
    #endif
    
    // Windows 文件 open 需要 O_BINARY 以避免 CRLF 转换
    static inline int fs_open(const char* path, int flags, int mode) {
        // 确保添加 O_BINARY 标志
        flags |= O_BINARY;
        return _open(path, flags, mode);
    }
    
    static inline int fs_close(int fd) {
        return _close(fd);
    }
    
    static inline off_t fs_lseek(int fd, off_t offset, int whence) {
        return _lseeki64(fd, offset, whence);
    }
    
    // 使用 fs_open 替代 open
    #define open(path, flags, mode) fs_open(path, flags, mode)
    #define close(fd) fs_close(fd)
    #define lseek(fd, offset, whence) fs_lseek(fd, offset, whence)
    
    /**
     * ftruncate - 截断/扩展文件到指定大小
     * Windows 使用 _chsize_s 实现
     */
    static inline int ftruncate(int fd, off_t length) {
        return _chsize_s(fd, length);
    }
    
#else
    // Linux/Unix 平台
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
#endif

#endif // FS_PLATFORM_COMPAT_H
