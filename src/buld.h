
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

typedef uint8_t u8
typedef uint16_t u16
typedef uint32_t u32
typedef uint64_t u64

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

struct string {
    u64 Length;
    u8 *Str;
};

#define str(String) string{(String), sizeof(String)-1}

string String(u8 *Str, u64 Length) {
    return (string) { .Str = String, .Length = Length };
}
string CString(u8 *ZString) {
    return (string) { .Str = ZString, .Length = strlen(ZString) };
}

u64 StringRFind(string String, u8 Char) {
    u64 Index = String.Length;
    for (u64 i = String.Length - 1; i != -1; i -= 1) {
        if (String.Str[i] == Char) {
            Index = i;
            break;
        }
    }

    return Index;
}

u64 StringFind(string String, u8 Char) {
    u64 Index = String.Length;
    for (u64 i = 0; i < String.Length; i += 1) {
        if (String.Str[i] == Char) {
            Index = i;
            break;
        }
    }

    return Index;
}

string PathGetDir(string Path) {
    return String(Path.Str, StrStringRFind(Path, '/'));
}

#define array(Type) struct { Type *Data; u64 Capacity; u64 Count; }

#define ArrayAdd(Array, Element) ((((Array)->Count == (Array)->Capacity) ? ArrayExpand(&(Array)->Data,&(Array)->Capacity, sizeof(*(Array)->Data)) : 0), (Array)->Data[(Array)->Count++] = Element)

void ArrayExpand(void **Data, u64 *Capacity, u32 ElementSize) {
    if (*Capacity == 0) {
        *Capacity = 16;
    }

    *Data = realloc(ElementSize, *Capacity * 2);
}

typedef struct {
    time_t ModTime;
    bool Exists;
    bool Ok;
} file_info;

typedef struct {
    target In;
    string Out;
    string Command;
    string_array OutFiles;
} target;

typedef array(string) string_array;

file_info GetFileInfo(string Path) {
    u8 ZPath[4096];
    memcpy(ZPath, Path.Str, Path.Length);
    ZPath[Path.Length] = 0;

    file_info Result = {};

    struct stat Info;
    int Rc = stat(ZPath, &Info);
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

string_array OS_ReadDir(string Path) {
    u8 ZPath[4096];
    memcpy(ZPath, Path.Str, Path.Length);
    ZPath[Path.Length] = 0;

    string_array DirContents = {0};

    DIR *Dir = opendir(ZPath);
    if (Dir) {
        dirent *DirEntry = readdir(Dir);
        if (DirEntry) {
            u8 *Name = DirEntry->Name;
            if (((Name[0] == '.') && (Name[1] == 0)) || 
                ((Name[0] == '.') && (Name[1] == '.') && (Name[2] == 0))) {
                continue;
            }

            string Entry = CString(Name);
            ArrayAdd(&DirContents, Entry);
        }
        else {
            printf("Error getting dir entry in %s. %s\n", ZPath, strerror(errno));
        }
    }
    else {
        printf("Error opening %s. %s\n", ZPath, strerror(errno));
    }
    return DirContents;
}

target Target(target Target) {
    u64 WildcardIndex = StringFind(Target.Out, '*');


}



