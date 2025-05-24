#pragma once

#include <stdint.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

//-----------------------------------------------------------------------------
// Base layer
//

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;

#define Min(a, b) (((a) < (b)) ? (a) : (b))
#define Max(a, b) (((a) > (b)) ? (a) : (b))

#define array(Type) struct { Type *Data; u64 Capacity; u64 Count; }

#define ArrayAdd(Array, Element) ( \
    (((Array)->Count == (Array)->Capacity) ? \
        ArrayExpand((void **)&(Array)->Data,&(Array)->Capacity, sizeof(*(Array)->Data)) : 0), (Array)->Data[(Array)->Count++] = Element)

void *ArrayExpand(void **Data, u64 *Capacity, u32 ElementSize) {
    if (*Capacity == 0) {
        *Capacity = 16;
    }
    else {
        *Capacity *= 2;
    }

    *Data = realloc(*Data, ElementSize * *Capacity);
    return *Data;
}

typedef struct {
    u64 Length;
    u8 *Str;
} string;

typedef array(string) string_list;

#define Str(String) (string){.Str = ((u8 *)(String)), .Length = (sizeof(String)-1)}
#define Strval(String) (int)((String).Length), (String).Str

// Is this allowed?
#define StrArr(String) ((string_array){ .Data = &Str(String), .Count = 1, .Capacity = 0 })

string String(u8 *Str, u64 Length) {
    return (string) { .Str = Str, .Length = Length };
}
string CString(u8 *ZString) {
    return (string) { .Str = ZString, .Length = strlen((char *)ZString) };
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
    return String(Path.Str, StringRFind(Path, '/'));
}

string_list FindReplace(string_list Strings, string FindPattern, string ReplacePattern) {
    string_list Result = {0};
    assert(0);
    return Result;
}

//-----------------------------------------------------------------------------
// OS
//

typedef struct {
    s64 ModTime;
    bool Exists;
    bool Ok;
} os_file_info;


os_file_info OS_GetFileInfo(string Path) {
    u8 ZPath[4096];
    memcpy(ZPath, Path.Str, Path.Length);
    ZPath[Path.Length] = 0;

    os_file_info Result = {0};

    struct stat Info;
    int Rc = stat((char *)ZPath, &Info);
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

string_list OS_ReadDir(string Path) {
    u8 ZPath[4096];
    memcpy(ZPath, Path.Str, Path.Length);
    ZPath[Path.Length] = 0;

    string_list DirContents = {0};

    DIR *Dir = opendir((char *)ZPath);
    if (Dir) {
        for (struct dirent *DirEntry = readdir(Dir); 
             DirEntry; 
             DirEntry = readdir(Dir)) {
            u8 *Name = (u8 *)DirEntry->d_name;
            if (((Name[0] == '.') && (Name[1] == 0)) || 
                ((Name[0] == '.') && (Name[1] == '.') && (Name[2] == 0))) {
                continue;
            }

            string Entry = CString(Name);
            ArrayAdd(&DirContents, Entry);
        }
    }
    else {
        fprintf(stderr, "Error opening %s. %s\n", ZPath, strerror(errno));
    }

    return DirContents;
}

s32 OS_RunCommand(string Command, string Out, string_list In) {
    return 0;
}

//-----------------------------------------------------------------------------
// Incremental build
//

struct target;

typedef array(struct target) target_list;

typedef struct target {
    target_list Prereqs;
    string Name;
    bool NeedsRebuild;
    s64 ModTime; 
} target;

string_list ToStringList(target_list TargetList) {
    string_list StringList = {0};
    for (u64 i = 0; i < TargetList.Count; i += 1) {
        ArrayAdd(&StringList, TargetList.Data[i].Name);
    }
    return StringList;
}

target_list Sources(string_list Names) {
    target_list TargetList = {0};
    for (u64 i = 0; i < Names.Count; i += 1) {
         string Name = Names.Data[i];
         os_file_info FileInfo = OS_GetFileInfo(Name);
         if (!FileInfo.Ok) {
            fprintf(stderr, "Unable to stat file %.*s\n", Strval(Name));
            return (target_list){0};
         }

         if (!FileInfo.Exists) {
            TargetList = (target_list){0};
            break;
         }

         target T = { .Name = Names.Data[i], .ModTime = FileInfo.ModTime };
         ArrayAdd(&TargetList, T);
    }

    return TargetList;
}

// main1: obj1 obj2 obj3

// obj1 : cpp1
// obj2 : cpp2
// obj3 : cpp3

// Need cycle detection? Maybe just rely on user not doing that
// Handling failed commands
// Target with no prereqs isn't useful they can just have a cmdline and run command thing
// for that. 

target Target(string TargetName, target_list Prereqs) {
    // This assumes that all prereqs already exist
    bool NeedsRebuild = true;
    s64 TargetModTime = INT64_MAX;
    if (Prereqs.Count > 0) {
        NeedsRebuild = false;
        os_file_info FileInfo = OS_GetFileInfo(TargetName);
        if (!FileInfo.Ok) {
            fprintf(stderr, "Unable to stat file %.*s\n", Strval(TargetName));
            return (target){0};
        }

        if (FileInfo.Exists) {
            for (u64 i = 0; i < Prereqs.Count; i += 1) {
                target *Prereq = Prereqs.Data + i;
                if (Prereq->NeedsRebuild || FileInfo.ModTime < Prereq->ModTime) {
                    NeedsRebuild = true;
                    break;
                }
            }
            TargetModTime = FileInfo.ModTime;
        }
    }

    target Target = { 
        .Name = TargetName, 
        .Prereqs = Prereqs, 
        .NeedsRebuild = NeedsRebuild, 
        .ModTime = TargetModTime,
    };

    return Target;
}

target_list TargetList(string_list Targets, target_list Prereqs) {

    // This assumes that all prereqs already exist
    target_list TargetList = {0};
    for (u64 i = 0; i < Targets.Count; i += 1) {
        target_list Prereq = {0};
        ArrayAdd(&Prereq, Prereqs.Data[i]);
        target T = Target(Targets.Data[i], Prereq);
        ArrayAdd(&TargetList, T);
    }

    return TargetList;
}

target *TargetIterNext(target *It, target_list *TargetList) {
    target *Result = 0;
    if (TargetList->Count > 0) {
        target *Begin = (It) ? (It + 1) : TargetList->Data;
        target *End = TargetList->Data + TargetList->Count;
        for (target *It = Begin; It != End; It += 1) {
            if (It->NeedsRebuild) {
                Result = It;
                break;
            }
        }
    }

    return Result;
}



#if 0
// TODO: Its probably better to first just recurse to leaf nodes, then check if then need 
// to be rebuilt and scan upwards. 
// This wastes work attempting to check root nodes against intermediate nodes times, when
// a changed leaf node would have forced a rebuild anyway.
bool BuildTarget2(target Target) {

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
    for (u64 i = 0; i < Target.In.Count; i += 1) {
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
        bool RebuiltInTarget = BuildTarget2(InTarget);
        if (RebuiltInTarget) {
            TargetNeedsBuild = true;
        }
    }
     

    if (TargetNeedsBuild) {
        RunCommand(Target.Command);
    }

    return Rebuilt;
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

        for (u64 i = 0; i < Out.Count; i += 1) {
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
        for (u64 i = 0; i < In.Count; i += 1) {
            // 1 out < 1 in
            target_list OneIn = {0};
            ArrayAdd(&OneIn, In.Data[i]);
            target T = { .In = OneIn, .Out = Out.Data[i], .Command = Command };
            ArrayAdd(&TargetList, T);
        }
    }

    return TargetList;
}

#endif


