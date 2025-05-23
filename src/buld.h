
#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

typedef uint8_t  u8
typedef uint16_t u16
typedef uint32_t u32
typedef uint64_t u64
typedef int8_t   s8
typedef int16_t  s16
typedef int32_t  s32
typedef int64_t  s64

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define array(Type) struct { Type *Data; u64 Capacity; u64 Count; }

#define ArrayAdd(Array, Element) ((((Array)->Count == (Array)->Capacity) ? ArrayExpand(&(Array)->Data,&(Array)->Capacity, sizeof(*(Array)->Data)) : 0), (Array)->Data[(Array)->Count++] = Element)

void ArrayExpand(void **Data, u64 *Capacity, u32 ElementSize) {
    if (*Capacity == 0) {
        *Capacity = 16;
    }

    *Data = realloc(ElementSize, *Capacity * 2);
}

struct string {
    u64 Length;
    u8 *Str;
};

typedef array(string) string_list;

#define Str(String) string{(String), sizeof(String)-1}
#define Strval(String) (int)(String).Length, (String).Data

// Is this allowed?
#define StrArr(String) ((string_array){ .Data = &Str(String), .Count = 1, .Capacity = 0 })

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

string_array FindReplace(string_array Strings, string FindPattern, string ReplacePattern) {
    string_array Result = {0};
    assert(0);
    return Result;
}

typedef struct {
    s64 ModTime;
    bool Exists;
    bool Ok;
} file_info;

typedef struct {
    target_list In;
    string Out;
    string Command;
} target;

typedef array(target) target_list;

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
            fprintf(stderr, "Error getting dir entry in %s. %s\n", ZPath, strerror(errno));
        }
    }
    else {
        fprintf(stderr, "Error opening %s. %s\n", ZPath, strerror(errno));
    }

    return DirContents;
}

target Target(target In, string InPattern, string OutPattern, string Command) {
    return (target){0};
}

// 1 out < 0 in (leaf node)
// 1 out < 1 in
// 1 out <- many in 
// many out < many in
// many out <- 1 in

target_list TargetList(target_list In, string_list Out, string Command) {
    target_list TargetList = {0};
    if (In.Count == 0) {
        // leaf node e.g. source file

        for (u32 i = 0; i < Out.Count; i += 1) {
            // 1 out < 1 in
            target T = { .In = {0}, .Out = Out.Data[i], .Command = Command };
            ArrayAdd(&TargetLIst, T);
        }
    }
    else if (Out.Count == 1) {
        // 1 out < 1 in
        // 1 out <- many in 
        target T = { .In = In, .Out = Out.Data[i], .Command = Command };
        ArrayAdd(&TargetList, T);
    }
    else {
        assert(In.Count == Out.Count);

        // many out < many in
        for (u32 i = 0; i < In.Count; i += 1) {
            // 1 out < 1 in
            target_list OneIn = {0};
            ArrayAdd(&OneIn, In.Data[i]);
            target T = { .In = OneIn, .Out = Out.Data[i], .Command = Command };
            ArrayAdd(&TargetList, T);
        }
    }

    return TargetList;
}


// main1: obj1 obj2 obj3

// obj1 : cpp1
// obj2 : cpp2
// obj3 : cpp3

// TODO: Its probably better to first just recurse to leaf nodes, then check if then need 
// to be rebuilt and scan upwards. 
bool BuildTarget(target Target) {

    string OutPath = Target.Out;
    file_info OutFileInfo = GetFileInfo(OutPath);
    if (!OutFileInfo.Ok) {
        fprintf("Unable to stat file %.*s\n", Strval(OutPath));
        return false;
    }

    if (!OutFileInfo.Exists && Target.In.Count == 0) {
        // We could make this always run command and reutn true
        fprintf(stderr, "Unable to find file %.*s\n", Strval(Target.Out));
        return false;
    }

    s64 OutModTime = (OutFileInfo.Exists) ? OutFileInfo.ModTime : INT64_MIN;

    bool TargetNeedsBuild = !OutFileInfo.Exists;
    for (u32 i = 0; i < Target.In.Count; i += 1) {
        target InTarget Target.In.Data[i];
        string InPath = InTarget.Out;
        file_info InFileInfo = GetFileInfo(InPath);
        if (!InFileInfo.Ok) {
            fprintf("Unable to stat file %.*s\n", Strval(InPath));
        }

        s64 InModTime = InFileInfo.ModTime;

        if (!InFileInfo.Exists || InModTime > OutModTime) {
            TargetNeedsBuild  = true;
        }

        // TODO: Pass through InTarget Timestamp to avoid re-querying
        bool RebuiltInTarget = BuildTarget(InTarget);
        if (!InFileInfo.Exists) {
            if (RebuiltInTarget) {
                TargetNeedsBuild = true;
            }
        }
    }
     

    if (TargetNeedsBuild) {
        RunCommand(Target.Command);
    }

    return Rebuilt;
}


