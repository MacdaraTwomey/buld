
#include <stdint.h>
#include <errno.h>
#include <time.h>

#include <sys/stat.h>

typedef uint8_t u8
typedef uint16_t u16
typedef uint32_t u32
typedef uint64_t u64

struct string {
    u64 Length;
    u8 *Str;
};

#define str(String) string{(String), sizeof(String)-1}

struct file_info {
    time_t ModTime;
    bool Exists;
    bool Ok;
};

file_info GetFileInfo(u8 Path) {
    file_info Result = {};

    struct stat Info;
    int Rc = stat(Path, &Info);
    if (Rc == 0) {
        Result.ModTime = Info.st_mtime;
        Result.Exists = true;
        Result.Ok = true;
    }
    else if (Rc == -1 && errno == ENOENT) {
        Result.Ok = true;
    }

    return Result;
}




