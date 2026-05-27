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
#include <stdarg.h>

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

void *AllocSize(u64 Size) {
    return calloc(Size, 1);
}

#define PushArray(Count, Type) ((Type *)AllocSize((Count) * sizeof(Type)))
#define PushStruct(Type) ((Type *)AllocSize(sizeof(Type)))

template <typename T, size_t n>
constexpr size_t ArrayCount(T(&)[n])
{
        return n;
}

#if BASE_COMPILER_CLANG || BASE_COMPILER_GCC
// 1 based indexing for parameter
#  define BASE_FORMAT_STRING_CHECK(StringIndex, ArgsIndex)  __attribute__((__format__ (__printf__, StringIndex, ArgsIndex)))
#else
#  define BASE_FORMAT_STRING_CHECK(StringIndex, ArgsIndex)
#endif

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

struct string {
    u64 Length;
    u8 *Str;
};

#define StrArg(String) ((int)((String).Length)), ((String).Str)
#define Strlit(String) (string{ sizeof(String)-1, (u8 *)String })


typedef array(string) string_list;

typedef array(u8 *) zstring_list;

#define Str(String) string{(sizeof(String)-1), (u8 *)String }

string CreateString(u8 *Str, u64 Length) {
    return { Length, Str };
}

string CreateString(char *Str) {
    return { strlen(Str), (u8 *)Str };
}

string CreateString(u8 *Str) {
    return { strlen((char *)Str), (u8 *)Str };
}

string StringPrefix(string String, u64 Length) {
    String.Length = Min(Length, String.Length);
    return String;
}

string StringSkip(string String, u64 Length) {
    u64 SkipLength = Min(Length, String.Length);
    String.Length -= SkipLength;
    String.Str += SkipLength; 
    return String;
}

string SubstrRange(string String, u64 First, u64 OnePastLast) {
    assert(OnePastLast >= First);
    First = Min(First, String.Length);
    OnePastLast = Min(OnePastLast, String.Length);
    
    String.Str += First;
    String.Length = OnePastLast - First;
    return String;
}

u8 StringGetChar(string String, u64 Index)
{
    u8 Result = 0;
    if (Index < String.Length)
    {
        Result = String.Str[Index];
    }

    return Result;
}

bool StringMatch(string A, string B) {
    bool Match = A.Length == B.Length;
    if (Match) {
        for (u64 i = 0; i < A.Length; i += 1) {
            if (A.Str[i] != B.Str[i]) {
                Match = false;
                break;
            }
        }
    }

    return Match;
}

bool StringStartsWith(string String, string PrefixString) {
    return StringMatch(StringPrefix(String, PrefixString.Length), PrefixString);
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

u64 StringFind(string String, u8 Char, u64 Start = 0);
u64 StringFind(string String, u8 Char, u64 Start) {
    u64 Index = String.Length;
    for (u64 i = Start; i < String.Length; i += 1) {
        if (String.Str[i] == Char) {
            Index = i;
            break;
        }
    }

    return Index;
}

u64 StringFindStr(string Haystack, string Needle, u64 Start = 0);

u64 StringFindStr(string Haystack, string Needle, u64 Start) {
    Start = Min(Start, Haystack.Length);
    u64 Position = Haystack.Length;

    if (Needle.Length > 0 && Haystack.Length >= Needle.Length) {
        for (u64 i = Start; i < Haystack.Length - Needle.Length + 1; ++i) {
            if (Needle.Str[0] == Haystack.Str[i]) {
                bool Found = true;
                for (u64 j = 1; j < Needle.Length; ++j) {
                    if (Needle.Str[j] != Haystack.Str[i + j]) {
                        Found = false;
                        break;
                    }
                }

                if (Found) {
                    Position = i;
                    break;
                }
            }
        }
    }

    return Position;
}

u64 StringFindChar(string String, u8 Char, u64 Start=0);

u64 StringFindChar(string String, u8 Char, u64 Start)
{
    u64 Position = String.Length;
    for (u64 i = Start; i < String.Length; ++i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }

    return Position;
}

struct cut_result {
    string Before;
    string After;
    bool Found;
};

cut_result StringCut(string String, string Separator) {
    cut_result Result = {};

    u64 Index = StringFindStr(String, Separator);
    if (Index < String.Length) {
        Result = {
            .Before = StringPrefix(String, Index),
            .After = StringSkip(String, Index + Separator.Length),
            .Found = true,
        };
    }
    else {
        Result = {
            .Before = String,
            .After = Strlit(""),
            .Found = false,
        };
    }

    return Result;
}

string PathGetDir(string Path) {
    return CreateString(Path.Str, StringRFind(Path, '/'));
}

template<typename... ts>
string StringConcat(ts... Args)
{
    string Strings[] = {string{}, Args...};

    u64 Length = 0;
    for (u64 i = 1; i < ArrayCount(Strings); ++i)
    {
        Length += Strings[i].Length;
    }

    u64 CopiedLength = 0;
    u8 *StringMemory = (u8 *)calloc(Length, 1);
    for (u64 i = 1; i < ArrayCount(Strings); ++i)
    {
        if (Length > 0)
        {
            memcpy(StringMemory + CopiedLength, Strings[i].Str, Strings[i].Length);
            CopiedLength += Strings[i].Length;
        }
    }

    return CreateString(StringMemory, CopiedLength);
}

// TODO: Make this better if not using arena
string PushStringfArgs(char *Format, va_list Args) BASE_FORMAT_STRING_CHECK(1, 2)
{
    u64 Length = 0;
    u64 Capacity = 1024;
    u8 *Buffer = PushArray(Capacity, u8);

    // We have to copy the va_list because we may call vsnprintf twice
    va_list ArgsCopy;
    va_copy(ArgsCopy, Args);

    int RequiredCharCount = vsnprintf((char *)Buffer, Capacity, Format, Args);
    if (RequiredCharCount < 0)
    {
        // Error
        Length = 0;
        assert(0);
    }
    else if ((u64)RequiredCharCount >= Capacity)
    {
        // Truncated
        // Returns the number of chars that would have been written (not including the null terminator).
        // If RequiredCharCount == StringLength (not including null terminator) then the last char was not
        // written as a null terminator must be written instead.

        u64 NewCapacity = (u64)RequiredCharCount + 1;
        Buffer = PushArray(NewCapacity, u8);

        int WrittenCharCount = vsnprintf((char *)Buffer, NewCapacity, Format, ArgsCopy);
        if (WrittenCharCount < 0)
        {
            Length = 0;
            assert(0);
        }
        else
        {
            // Leave null terminated in allocated string
            Length = (u64)WrittenCharCount;
        }
    }
    else
    {
        // Success
        Length = (u64)RequiredCharCount;
    }

    va_end (ArgsCopy);

    return CreateString(Buffer, Length);
}


string PushStringf(char *Format, ...)
{
    va_list Args;
    va_start (Args, Format);
    string Result = PushStringfArgs(Format, Args);
    va_end (Args);

    return Result;
}

struct string_node
{
    string String;
    string_node *Next;
};

struct s_list
{
    string_node *Head;
    string_node *Tail;
    u64 Length;
    u64 Count;
};

void StringListAppend(s_list *List, string String)
{
    string_node *Node = PushStruct(string_node);
    Node->String = String;
    Node->Next = nullptr;

    if (List->Head == nullptr)
    {
        List->Head = Node;
    }
    else
    {
        List->Tail->Next = Node;
    }

    List->Tail = Node;
    List->Length += String.Length;
    List->Count += 1;
}

void StringListAppend(s_list *List, char *String)
{
    StringListAppend(List, CreateString(String));
}

string StringListJoin(s_list *List)
{
    u8 *StringMemory = PushArray(List->Length, u8);

    u64 CopiedLength = 0;
    for (string_node *Node = List->Head; Node; Node = Node->Next)
    {
        if (Node->String.Length > 0)
        {
            memcpy(StringMemory + CopiedLength, Node->String.Str, Node->String.Length);
            CopiedLength += Node->String.Length;
        }
    }

    return CreateString(StringMemory, CopiedLength);
}

string FindReplace(string String, string MatchString, string ReplaceString) {
    u64 Index = StringFindStr(String, MatchString);
    string Before = StringPrefix(String, Index);
    string After = StringSkip(String, Index + MatchString.Length);
    string Result = StringConcat(Before, ReplaceString, After);
    return Result;
}

bool IsNumber(u8 c)
{
    return (('0' <= c) && (c <= '9'));
}

struct parsed_num
{
    u64 Value;
    bool Error;
};

parsed_num StringParseNum(string String)
{
    parsed_num Result = {};
    Result.Error = (String.Length == 0);

    for (u64 i = 0; i < String.Length; ++i)
    {
        u8 c = String.Str[i];
        if (!IsNumber(c))
        {
            Result.Value = 0;
            Result.Error = true;
            return Result;
        };

        u64 NewValue = (Result.Value * 10) + (c - '0');
        if (NewValue < Result.Value)
        {
            Result.Value = 0;
            Result.Error = true;
            return Result;
        }

        Result.Value = NewValue;
    }

    return Result;
}


#if 0

string FindReplace(string String, string FindPattern, string ReplacePattern, string MatchPattern) {
    u64 FindWildcardCharIndex = StringFindStr(FindPattern, MatchPattern);
    string FindPrefix = FindPattern;
    string FindSuffix = {};
    if (FindWildcardCharIndex < FindPattern.Length) {
        FindPrefix = StringPrefix(FindPattern, FindWildcardCharIndex);
        FindSuffix = StringSkip(FindPattern, FindWildcardCharIndex+1);
    }

    u64 ReplaceWildcardCharIndex = StringFindStr(ReplacePattern, MatchPattern);
    string ReplacePrefix = ReplacePattern;
    string ReplaceSuffix = {};
    if (ReplaceWildcardCharIndex < ReplacePattern.Length) {
        ReplacePrefix = StringPrefix(ReplacePattern, ReplaceWildcardCharIndex);
        ReplaceSuffix = StringSkip(ReplacePattern, ReplaceWildcardCharIndex+1);
    }

    //                     v   v   v
    //            012345678901234567890
    // string:    projects/src/main.c
    // pattern:   src/%.c
    // findpref:  src/
    // findsuff:  .c
    u64 BeforeMatchLength = 0;
    u64 PrefixIndex = StringFindStr(String, FindPrefix);
    if (PrefixIndex < String.Length) {
        BeforeMatchLength = PrefixIndex + FindPrefix.Length;
    }

    u64 SuffixIndex = StringFindStr(String, FindSuffix, BeforeMatchLength);

    string Match = SubstrRange(String, BeforeMatchLength, SuffixIndex);

    string Result = StringConcat(ReplacePrefix, Match, ReplaceSuffix);

    return Result;
}

string_list FindReplaceList(string_list Strings, string FindPattern, string ReplacePattern) {
    string_list ReplacementStrings = {};
    for (u64 i = 0; i < Strings.Count; i += 1) {
        string String = Strings.Data[i];
        string Replacement = FindReplace(String, FindPattern, ReplacePattern, Str("%"));
        ArrayAdd(&ReplacementStrings, Replacement);
    }

    return ReplacementStrings;
}
#endif

void CopyString(u8 *Buffer, string String) {
    memcpy(Buffer, String.Str, String.Length);
    Buffer[String.Length] = 0;
}


template<class T> struct remove_reference { typedef T type; };
template<class T> struct remove_reference<T&> { typedef T type; };
template<class T> struct remove_reference<T&&> { typedef T type; };
template< class T >
using remove_reference_t = typename remove_reference<T>::type;

struct true_type {
    static constexpr bool value = true;
    constexpr operator bool() const noexcept { return value; }
};

struct false_type {
    static constexpr bool value = false;
    constexpr operator bool() const noexcept { return value; }
};

template<class T> struct is_lvalue_reference     : false_type {};
template<class T> struct is_lvalue_reference<T&> : true_type {};
template< class T >
constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
constexpr T&& forward(remove_reference_t<T>& t) noexcept {
    return static_cast<T&&>(t);
}

template <typename T>
constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
    static_assert(!is_lvalue_reference_v<T>, "cannot forward rvalue as lvalue");
    return static_cast<T&&>(t);
}

//-----------------------------------------------------------------------------
// OS
//

// TODO: Push an error if this fails
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

            string Entry = CreateString(Name);
            ArrayAdd(&DirContents, Entry);
        }
    }
    else {
        fprintf(stderr, "Error opening %s. %s\n", ZPath, strerror(errno));
    }

    return DirContents;
}

struct read_entire_file_result {
    string String;
    bool Error;
    bool NotExist;
};

read_entire_file_result OS_ReadEntireFile(string Path) {
    read_entire_file_result Result = {};

    u8 ZPath[4096];
    memcpy(ZPath, Path.Str, Path.Length);
    ZPath[Path.Length] = 0;

    FILE *File = fopen((char *)ZPath, "rb");
    if (File) {
        int SeekResult = fseek(File, 0, SEEK_END);
        long Size = ftell(File);
        SeekResult |= fseeko(File, 0, SEEK_SET);
        if (SeekResult == 0 && Size != -1) {
            u8 *Buffer = PushArray(Size, u8);

            size_t ReadSize = fread(Buffer, 1, Size, File);
            if (ReadSize == Size) {
                Result.String = CreateString(Buffer, Size);
            }
            else {
                Result.Error = true;
            }
        }
        else {
            Result.Error = true;
        }
        fclose(File);
    }
    else {
        if (errno == ENOENT) {
            Result.NotExist = true;
        }
        else {
            Result.Error = true;
        }
    }

    return Result;
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

s32 OS_RunProcess(string Program, string *Args, s32 ArgCount) {
    s32 ReturnCode = -1;

    zstring_list Argv = {};

    u8 *ZProgram = (u8 *)calloc(Program.Length + 1, 1);
    CopyString(ZProgram, Program);

    ArrayAdd(&Argv, ZProgram);

    for (s32 i = 0; i < ArgCount; i += 1) {
        string Arg = Args[i];
        u8 *ZArg = (u8 *)calloc(Arg.Length + 1, 1);
        CopyString(ZArg, Arg);
        ArrayAdd(&Argv, ZArg);
    }

    ArrayAdd(&Argv, (u8 *)0);

    pid_t PID = fork();
    if (PID == -1) {
    }
    else if (PID == 0) {
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
                fprintf(stdout, "Program %.*s signalled %d (%s)\n", StrArg(Program), Signal, strsignal(Signal));
            }
        }
    }

    return ReturnCode;
}

//-----------------------------------------------------------------------------
// build library
//

struct target;

struct target_list {
    target **Elements;
    u64 Count;

    target_list() = default;
    target_list(const char *String);
    target_list(string String);
    target_list(target *Target);
};

struct target {
    string Path;
    os_file_info FileInfo;
    target *ParentCommand;

    target_list Input;
    target_list Output;
    string Args;
    string Program;
    target_list Depends;

    bool NeedsRebuild;
};

struct string_arg {
    string Data;
    string_arg() = default;
    string_arg(const char* String) : Data(CreateString((char *)String)) {}
    string_arg(string String) : Data(String) {}
};

struct target_args {
    string_arg Path;
    target_list Input;
    target_list Output;
    string_arg Args;
    string_arg Program;
    target_list Depends;
};

struct state {
    target Targets[10000];
    u64 TargetCount;
    string Errors[10000];
    u64 ErrorCount;
    string ProjectRoot;
    target *CommandsToRun[10000];
    u64 CommandsToRunCount;
};

state State = {};

void PushError(string String) {
    State.Errors[State.ErrorCount++] = String;
}

void PushError(char *Format, ...) BASE_FORMAT_STRING_CHECK(1, 2) {
    va_list Args;
    va_start (Args, Format);
    string Result = PushStringfArgs(Format, Args);
    va_end (Args);
    PushError(Result);
}


struct cmd_variable {
    string Name;
    string Value;
};

typedef array(cmd_variable) cmd_variable_array;

static cmd_variable_array GlobalVariables = {};

void DefineVariable(string Name, string Value) {
    cmd_variable Var = {};
    Var.Name = Name;
    Var.Value = Value;
    ArrayAdd(&GlobalVariables, Var);
}

struct get_variable_result {
    string Value;
    bool Found;
};

get_variable_result GetVariable(cmd_variable *Variables, u64 VariableCount, string Name) {
    get_variable_result Result = {};
    for (u64 i = 0; i < VariableCount; i += 1) {
        cmd_variable Var = Variables[i];
        if (StringMatch(Var.Name, Name)) {
            Result.Value = Var.Value;
            Result.Found = true;
            break;
        }
    }

    if (!Result.Found) {
        for (u64 i = 0; i < GlobalVariables.Count; i += 1) {
            cmd_variable Var = GlobalVariables.Data[i];
            if (StringMatch(Var.Name, Name)) {
                Result.Value = Var.Value;
                Result.Found = true;
                break;
            }
        }
    }

    return Result;
}

string ReplaceVars(string String, cmd_variable *Variables, u64 VariableCount) {
    u64 VariablesLength = 0;
    for (u64 i = 0; i < VariableCount; i += 1) {
        VariablesLength += Variables[i].Value.Length;
    }

    // TODO: Find a better way
    for (u64 i = 0; i < GlobalVariables.Count; i += 1) {
        VariablesLength += GlobalVariables.Data[i].Value.Length;
    }

    u8 *Buffer = (u8 *)calloc(VariablesLength + String.Length, 1);
    u64 Length = 0;
    for (u64 i = 0; i < String.Length; i += 1) {
        u8 c = String.Str[i];
        if (c == '{') {
            u64 CloseBraceIndex = StringFind(String, '}', i);
            if (CloseBraceIndex < String.Length) {
                string VariableName = SubstrRange(String, i + 1, CloseBraceIndex);
                get_variable_result Variable = GetVariable(Variables, VariableCount, VariableName);
                if (Variable.Found) {
                    memcpy(Buffer + Length, Variable.Value.Str, Variable.Value.Length);
                    Length += Variable.Value.Length;
                    i = CloseBraceIndex;
                    continue;
                }
                else {
                    fprintf(stdout, "Error: Undefined variable %.*s\n", StrArg(VariableName));
                }
            }
        }
        Buffer[Length++] = c;
    }

    return CreateString(Buffer, Length);
}

