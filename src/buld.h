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
#include <unistd.h>
#include <sys/wait.h> 

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

typedef array(u8 *) zstring_list;

#define Str(String) string{(sizeof(String)-1), (u8 *)String }
#define Strval(String) (int)((String).Length), (String).Str

string String(u8 *Str, u64 Length) {
    return { Length, Str };
}
string SubstrRange(string String, u64 First, u64 OnePastLast) {
    assert(OnePastLast >= First);
    First = Min(First, String.Length);
    OnePastLast = Min(OnePastLast, String.Length);
    
    String.Str += First;
    String.Length = OnePastLast - First;
    return String;
}
string CString(u8 *ZString) {
    return { strlen((char *)ZString), (u8 *)ZString };
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

void CopyString(u8 *Buffer, string String) {
    memcpy(Buffer, String.Str, String.Length);
    Buffer[String.Length] = 0;
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

struct arg_parser {
    u64 At;
    string String;
    bool Error;
};

string GetArg(arg_parser *Parser) {
    u8 QuoteChar = 0;
    u64 Start = Parser->String.Length;
    u64 End = Parser->String.Length;
    for (; Parser->At < Parser->String.Length; Parser->At += 1) {
        u8 C = Parser->String.Str[Parser->At];
        if (C != ' ') {
            Start = Parser->At;
            if (C == '\'' || C == '"') {
                QuoteChar = C;
                Parser->At += 1;
            }
            break;
        }
    }

    bool QuotesClosed = (QuoteChar == 0);
    for (; Parser->At < Parser->String.Length; Parser->At += 1) {
        u8 C = Parser->String.Str[Parser->At];
        if (QuoteChar) {
            // TODO: Handle escaping
            if (C == QuoteChar) {
                Parser->At += 1;
                End = Parser->At;
                QuotesClosed = true;
                break;
            }
        }
        else if (C == ' ') {
            End = Parser->At;
            break;
        }
    }

    if (!QuotesClosed) {
        Parser->Error = true;
    }

    return SubstrRange(Parser->String, Start, End);
}

s32 OS_RunCommand(string Command, string Out, string In) {
    s32 ReturnCode = -1;

    u8 ZCommand[4096];
    CopyString(ZCommand, Command);

    zstring_list Argv = {};
    arg_parser Parser = {};
    Parser.String = Command;
    string Program = GetArg(&Parser);
    if (Parser.Error || Program.Length == 0) {
        fprintf(stderr, "Error with command %.*s\n", Strval(Command));
        return -1;
    }

    u8 *ZProgram = (u8 *)calloc(Program.Length + 1, 1);
    CopyString(ZProgram, Program);

    ArrayAdd(&Argv, ZProgram);

    while (!Parser.Error) {
        string Arg = GetArg(&Parser);
        if (Arg.Length == 0) {
            break;
        }

        u8 *ZArg = (u8 *)calloc(Arg.Length + 1, 1);
        CopyString(ZArg, Arg);
        ArrayAdd(&Argv, ZArg);
    }

    ArrayAdd(&Argv, (u8 *)0);

    pid_t PID = fork();
    if (PID == -1) {
    }
    else if (PID == 0) {

        if (Parser.Error) {
            fprintf(stderr, "Error parsing command %.*s\n", Strval(Command));
            return -1;
        }

        if (execvp((char *)ZProgram, (char **)Argv.Data) < 0) {
            fprintf(stderr, "Child failed to exec()\n");
            exit(1);
        }
    }
    else {
        int ChildStatus = 0;
        int WaitResult = waitpid(PID, &ChildStatus, 0);
        if (WaitResult == -1) {
            // TODO: Handle failure of this, this might mean handling SIGCHLD in some way.
        }
        else {
            if (WIFEXITED(ChildStatus)) {
                int ExitStatus = WEXITSTATUS(ChildStatus);
                ReturnCode = ExitStatus;
            }
            else if (WIFSIGNALED(ChildStatus)) {
                // TODO Signal names
                int Signal = WTERMSIG(ChildStatus);
                fprintf(stdout, "Program %.*s signalled (%d)\n", Strval(Program), Signal);
            }
        }
    }

    return ReturnCode;
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
    bool Exists;
    s64 ModTime; 
    string Error;
    string Command;
} target;

string_list ToStringList(target_list TargetList) {
    string_list StringList = {0};
    for (u64 i = 0; i < TargetList.Count; i += 1) {
        ArrayAdd(&StringList, TargetList.Data[i].Name);
    }
    return StringList;
}

string ToString(string_list StringList, u8 Separator) {
    u64 Length = 0;
    for (u64 i = 0; i < StringList.Count; i += 1) {
        Length += StringList.Data[i].Length;
    }
    u8 *Buffer = (u8 *)calloc(1, Length);
    u64 At = 0;
    for (u64 i = 0; i < StringList.Count; i += 1) {
        string S = StringList.Data[i];
        memcpy(Buffer + At, S.Str, S.Length);
        At += S.Length;
    }

    return String(Buffer, Length);
}

#if 0
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
#endif




// TODO: Its probably better to first just recurse to leaf nodes, then check if then need 
// to be rebuilt and scan upwards. 
// This wastes work attempting to check root nodes against intermediate nodes times, when
// a changed leaf node would have forced a rebuild anyway.
bool BuildTarget(target *Target) {

    if (Target->Error.Length) {
        fprintf(stderr, "Error: %.*s\n", Strval(Target->Error));
        return false;
    }

    if (Target->Prereqs.Count == 0 && !Target->Exists) {
        fprintf(stdout, "Error: File %.*s does not exist\n", Strval(Target->Name));
        return false;
    }

    bool PrereqsUpToDate = true;
    bool TargetUpToDate = Target->Exists;
    for (u64 i = 0; i < Target->Prereqs.Count; i += 1) {
        target *Prereq = Target->Prereqs.Data + i;
        bool PrereqUpToDate = BuildTarget(Prereq);
        if (PrereqUpToDate) {
            if (Prereq->ModTime > Target->ModTime) {
                TargetUpToDate = false;
            }
        }
        else {
            TargetUpToDate = false;
            PrereqsUpToDate = false;
            break;
        }
    }

    // Could also add list of (target, prereqs) and return that to user
    if (PrereqsUpToDate && !TargetUpToDate && Target->Command.Length) {
        string_list PrereqsList = ToStringList(Target->Prereqs);
        string PrereqsString = ToString(PrereqsList, ' ');
        s32 ReturnCode = OS_RunCommand(Target->Command, Target->Name, PrereqsString); 

        if (ReturnCode != 0) {
            TargetUpToDate = false;
        }
    }

    return TargetUpToDate;
}

bool CheckOk(target Target) {
    if (Target.Error.Length) {
        fprintf(stderr, "Error: %.*s\n", Strval(Target.Error));
        return false;
    }

    if (Target.Prereqs.Count == 0 && !Target.Exists) {
        fprintf(stdout, "Error: File %.*s does not exist\n", Strval(Target.Name));
        return false;
    }


    for (u64 i = 0; i < Target.Prereqs.Count; i += 1) {
        target *Prereq = Target.Prereqs.Data + i;
        bool Ok = CheckOk(*Prereq);
        if (!Ok) {
            return false;
        }
    }

    return true;
}

target Target(string Name, target_list Prereqs, string Command) {
    os_file_info FileInfo = OS_GetFileInfo(Name);
    // TODO: Better error message
    string Error = (!FileInfo.Ok) ? Str("Unable to stat file") : Str("");

    target Target = {};
    Target.Name = Name;
    Target.Prereqs = Prereqs;
    Target.Command = Command;
    Target.ModTime = FileInfo.ModTime;
    Target.Exists = FileInfo.Exists;
    Target.NeedsRebuild = false;
    Target.Error = Error;
    return Target;
}

target_list TargetList(string_list Targets, target_list Prereqs, string Command) {
    target_list TargetList = {0};
    if (Prereqs.Count == 0) {
        // leaf node e.g. source file
        for (u64 i = 0; i < Targets.Count; i += 1) {
            target T = Target(Targets.Data[i], Prereqs, Command);
            ArrayAdd(&TargetList, T);
        }
    }
    else if (Targets.Count == 1) {
        // 1 out < 1 in
        // 1 out <- many in 
        target T = Target(Targets.Data[0], Prereqs, Command);
        ArrayAdd(&TargetList, T);
    }
    else {
        assert(Targets.Count == Prereqs.Count);

        // many out < many in
        for (u64 i = 0; i < Targets.Count; i += 1) {
            // 1 out < 1 in
            target_list OnePrereq = {};
            ArrayAdd(&OnePrereq, Prereqs.Data[i]);
            target T = Target(Targets.Data[i], OnePrereq, Command);
            ArrayAdd(&TargetList, T);
        }
    }

    return TargetList;
}



