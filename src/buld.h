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
#include <glob.h>

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


#define Min(a, b) ((a) < (b) ? (a) : (b))
#define Max(a, b) ((a) > (b) ? (a) : (b))
#define Clamp(Value, Low, High) (((Value) < (Low)) ? (Low) : ((Value) > (High)) ? (High) : (Value))
#define ClampTop(a, b)    Min((a), (b))
#define ClampBottom(a, b) Max((a), (b))

#define KB(n) ((n) * 1024LLU)
#define MB(n) KB((n) * 1024LLU)
#define GB(n) MB((n) * 1024LLU)
#define TB(n) GB((n) * 1024LLU)

#define Assert(cond) assert(cond)

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

#define ArrayAddImpl(Data, Count, Capacity, Element) ( \
    (((Count) == (Capacity)) ? \
        ArrayExpand((void **)&(Data), &(Capacity), sizeof(*(Data))) : 0), (Data)[(Count++)] = (Element))

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

u64 StringLength(u8 *CString)
{
    u64 Length = 0;
    while (*CString)
    {
        ++CString;
        Length += 1;
    }

    return Length;
}

string CreateString(u8 *CString)
{
    string Result;
    Result.Str = CString;
    Result.Length = StringLength(CString);
    return Result;
}

string CreateString(char *CString)
{
    return CreateString((u8 *)CString);
}

string CreateString(u8 *StringData, u64 Length)
{
    string Result;
    Result.Str = StringData;
    Result.Length = Length;
    return Result;
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

bool IsUpper(u8 c)
{
    return (('A' <= c) && (c <= 'Z'));
}

bool IsLower(u8 c)
{
    return (('a' <= c) && (c <= 'z'));
}

bool IsAlpha(u8 c)
{
    return IsLower(c) || IsUpper(c);
}

bool IsNumber(u8 c)
{
    return (('0' <= c) && (c <= '9'));
}

bool IsAlphaNumeric(u8 c)
{
    return IsAlpha(c) || IsNumber(c);
}

bool IsWhitespace(u8 c)
{
    return (c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == '\v');
}

bool IsSlash(u8 c)
{
    return (c == '\\' || c == '/');
}

u8 ToUpper(u8 c)
{
    return IsLower(c) ? c - 32u : c;
}

u8 ToLower(u8 c)
{
    return IsUpper(c) ? c + 32u : c;
}

void StringToLower(string *String)
{
    for (u64 i = 0; i < String->Length; ++i)
    {
        String->Str[i] = ToLower(String->Str[i]);
    }
}

void StringToUpper(string *String)
{
    for (u64 i = 0; i < String->Length; ++i)
    {
        String->Str[i] = ToUpper(String->Str[i]);
    }
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

string StringPrefix(string String, u64 N)
{
    String.Length = ClampTop(N, String.Length);
    return String;
}

string StringSuffix(string String, u64 N)
{
    u64 SuffixLength = Min(N, String.Length);
    String.Str += (String.Length - SuffixLength);
    String.Length = SuffixLength;
    return String;
}

string SubstrRange(string String, u64 First, u64 OnePastLast)
{
    Assert(OnePastLast >= First);
    //   v   v
    // abcdef
    First = ClampTop(First, String.Length);
    OnePastLast = ClampTop(OnePastLast, String.Length);

    String.Str += First;
    String.Length = OnePastLast - First;
    return String;
}

string Substr(string String, u64 First, u64 Length)
{
    return SubstrRange(String, First, First + Length);
}

string StringSkip(string String, u64 N)
{
    N = ClampTop(N, String.Length);
    String.Str = String.Str + N;
    String.Length -= N;
    return String;
}

string StringChop(string String, u64 N)
{
    N = ClampTop(N, String.Length);
    String.Length -= N;
    return String;
}

u64 StringCountOccurence(string String, u8 Char)
{
    u64 Count = 0;
    for (u64 i = 0; i < String.Length; ++i)
    {
        if (String.Str[i] == Char)
        {
            Count += 1;
        }
    }

    return Count;
}

// Offest has a default value of 0
u64 StringFindChar(string String, u8 Char, u64 Offset=0)
{
    u64 Position = String.Length;
    for (u64 i = Offset; i < String.Length; ++i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }

    return Position;
}

u64 StringFindLastChar(string String, u8 Char)
{
    u64 Position = String.Length;
    for (u64 i = String.Length - 1; i != static_cast<u64>(-1); --i)
    {
        if (String.Str[i] == Char)
        {
            Position = i;
            break;
        }
    }

    return Position;
}


u64 StringFindStr(string Haystack, string Needle)
{
    u64 Position = Haystack.Length;

    if (Needle.Length > 0 && Haystack.Length >= Needle.Length)
    {
        for (u64 i = 0; i < Haystack.Length - Needle.Length + 1; ++i)
        {
            if (Needle.Str[0] == Haystack.Str[i])
            {
                bool Found = true;
                for (u64 j = 1; j < Needle.Length; ++j)
                {
                    if (Needle.Str[j] != Haystack.Str[i + j])
                    {
                        Found = false;
                        break;
                    }
                }

                if (Found)
                {
                    Position = i;
                    break;
                }
            }
        }
    }

    return Position;
}

bool StringContainsChar(string String, u8 Char)
{
    return StringFindChar(String, Char) < String.Length;
}

bool StringContainsStr(string String, string Substr)
{
    // O(n^2)
    bool ContainsSubstr = false;
    if (String.Length >= Substr.Length && Substr.Length > 0)
    {
        u64 LastPossibleCharIndex = String.Length - Substr.Length;
        for (u64 i = 0; i <= LastPossibleCharIndex; ++i)
        {
            if (String.Str[i] == Substr.Str[0])
            {
                ContainsSubstr = true;
                for (u64 j = 1; j < Substr.Length; ++j)
                {
                    if (String.Str[i + j] != Substr.Str[j])
                    {
                        ContainsSubstr = false;
                        break;
                    }
                }

                if (ContainsSubstr) break;
            }
        }
    }

    return ContainsSubstr;
}

bool StringMatch(string A, string B)
{
    bool Result = (A.Length == B.Length);
    if (Result)
    {
        for (u64 i = 0; i < A.Length; ++i)
        {
            if (A.Str[i] != B.Str[i])
            {
                Result = false;
                break;
            }
        }
    }

    return Result;
}

bool StringMatchCaseInsensitive(string a, string b)
{
    bool Result = (a.Length == b.Length);
    if (Result)
    {
        for (u64 i = 0; i < a.Length; ++i)
        {
            if (ToLower(a.Str[i]) != ToLower(b.Str[i]))
            {
                Result = false;
                break;
            }
        }
    }

    return Result;
}

u64 StringFindLastSlash(string Path)
{
    u64 Position = Path.Length;
    for (u64 i = Path.Length - 1; i != (u64)-1; --i)
    {
        if (IsSlash(Path.Str[i]))
        {
            Position = i;
            break;
        }
    }

    return Position;
}

string RemoveExtension(string File)
{
    // Files can have multiple dots, and only last is the real extension.

    // If no dot is found Length stays the same
    u64 DotPosition = StringFindLastChar(File, '.');
    return StringPrefix(File, DotPosition);
}

// If Path has no slashes (it is not an absolute path) then a zero length string is returned
string FilenameFromPath(string Path)
{
    u64 LastSlash = StringFindLastSlash(Path);
    Path = StringSkip(Path, LastSlash + 1);
    return Path;
}

// If Path has no slashes then then a zero length string is returned
// Includes slash after directory
string DirectoryFromPath(string Path)
{
    string Directory = {};
    u64 LastSlash = StringFindLastSlash(Path);
    if (LastSlash < Path.Length)
    {
        Directory = StringPrefix(Path, LastSlash + 1);
    }

    return Directory;
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

enum os_file_type {
    OS_FileType_Invalid,
    OS_FileType_File,
    OS_FileType_Directory,
};

// TODO: Push an error if this fails
typedef struct {
    s64 ModTime;
    os_file_type Type;
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
        // Treat everything thats not a directory as a file
        Result.Type = S_ISDIR(Info.st_mode) ? OS_FileType_Directory : OS_FileType_File;
    }
    else if (Rc == -1 && errno == ENOENT) {
        Result.Ok = true;
    }
    else if (Rc == -1 && errno == ENOENT) {
        Result.Ok = false;
    }

    return Result;
}

struct os_directory_iterator {
    void *Handle;
    bool Error;
    bool Done;
};


struct os_filesystem_entry {
    string Name;
    os_file_type Type;
};

os_directory_iterator OS_DirectoryIterator(string DirectoryPath) {

    u8 *ZDirectoryPath = (u8 *)calloc(DirectoryPath.Length + 1, 1);
    CopyString(ZDirectoryPath, DirectoryPath);

    DIR *Dir = opendir((char *)ZDirectoryPath);
    os_directory_iterator Iter = {
        .Handle = (void *)Dir,
        .Error = Dir == 0,
        .Done = Dir == 0,
    };

    return Iter;
}

os_filesystem_entry OS_DirectoryIteratorNext(os_directory_iterator *Iter) {
    os_filesystem_entry FilesystemEntry = {};

    if (Iter->Done) {
        return FilesystemEntry;
    }

    for (struct dirent *Entry = readdir((DIR *)Iter->Handle);
         Entry != 0;
         Entry = readdir((DIR *)Iter->Handle)) {
        if ((Entry->d_name[0] == '.' && Entry->d_name[1] == 0) ||
            (Entry->d_name[0] == '.' && Entry->d_name[1] == '.' && Entry->d_name[2] == 0)) {
            continue;
        }

        // Apparently not all files types support returning the type
        if (Entry->d_type == DT_UNKNOWN) {
            // TODO:
            assert(!"Not handled yet");
        }
        else if (Entry->d_type == DT_DIR) {
            FilesystemEntry.Type = OS_FileType_Directory;
            FilesystemEntry.Name = PushStringf("%s/", Entry->d_name);
        }
        else {
            FilesystemEntry.Type = OS_FileType_File;
            FilesystemEntry.Name = PushStringf("%s", Entry->d_name);
        }

        break;
    }

    // TODO: Check errors

    if (FilesystemEntry.Name.Length == 0) {
        Iter->Done = true;
    }

    return FilesystemEntry;
}

void OS_DirectoryIteratorClose(os_directory_iterator *Iter) {
    closedir((DIR *)Iter->Handle);
    *Iter = {};
}

struct os_wildcard_file_iter {
    glob_t Glob;
    u64 At;
    bool Done;
    bool Error;
};

os_wildcard_file_iter OS_WildcardFileIter(string Pattern) {
    u8 *ZPattern = (u8 *)calloc(Pattern.Length + 1, 1);
    CopyString(ZPattern, Pattern);

    glob_t Glob = {};
    int Flags = GLOB_MARK; // add '/' to dirs
    int Result = glob((char *)ZPattern, Flags, NULL, &Glob);

    bool NoResults = Result == GLOB_NOMATCH;
    return {
        .Glob = Glob,
        .Done = NoResults,
        .Error = !NoResults && Result != 0,
    };
}

void OS_WildcardFileIterClose(os_wildcard_file_iter *Iter) {
    if (!Iter->Error) {
        globfree(&Iter->Glob);
    }
    *Iter = {};
}

string OS_WildcardFileIterNext(os_wildcard_file_iter *Iter) {
    string File = {};

    if (Iter->At < Iter->Glob.gl_pathc) {
        File = PushStringf("%s", Iter->Glob.gl_pathv[Iter->At++]);
    }
    else {
        Iter->Done = true;
    }

    return File;
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

bool OS_ReplaceCurrentProcess(string ProgramPath, char **Argv) {
    u8 *ZProgramPath = (u8 *)calloc(ProgramPath.Length + 1, 1);
    CopyString(ZProgramPath, ProgramPath);

    int ExecResult = execv((char *)ZProgramPath, Argv);
    if (ExecResult < 0) {
        fprintf(stdout, "Failed to exec %.*s. %s", StrArg(ProgramPath), strerror(errno));
        return false;
    }

    return true;
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

string OS_GetCurrentExecutablePath() {
    string ExecutablePath = {};

    pid_t PID = getpid();

    char ZPath[4096];
    snprintf(ZPath, sizeof(ZPath), "/proc/%d/exe", PID);
    // Allocates buffer that must be freed for you
    char *ResolvedPath = realpath(ZPath, 0);
    if (ResolvedPath) {
        ExecutablePath = CreateString(ResolvedPath);
    }
    else {
        fprintf(stdout, "Unable to resolve %s symlink. %s\n", ZPath, strerror(errno));
    }

    return ExecutablePath;
}


//-----------------------------------------------------------------------------
// build library
//

struct target;

template<typename t>
struct list;

template<typename t>
void ListAdd(list<t> *List, t Value);

template<typename t>
struct list {
    t *Data;
    u64 Count;
    u64 Capacity;

    list() = default;
    //list(const list &Other) : Data(Other.Data), Count(Other.Count), Capacity(Other.Capacity) {
    //    int y = 1;
    //};
    //list &operator=(const list &Other) {
    //    Data = Other.Data;
    //    Count = Other.Count;
    //    Capacity = Other.Capacity;
    //    return *this;
    //};
    //list(list &&Other) : Data(Other.Data), Count(Other.Count), Capacity(Other.Capacity) {
    //    int y = 1;
    //};
    //list &operator=(list &&Other) {
    //    Data = Other.Data;
    //    Count = Other.Count;
    //    Capacity = Other.Capacity;
    //    return *this;
    //};

    template<typename... args>
    list(args&&... Args) : Data(0), Count(0), Capacity(0) {
        (ListAdd(this, forward<args>(Args)), ...);
    }
};

struct target {
    string Name;
    string Path;
    os_file_info FileInfo;
    target *ParentCommand;

    list<target *> Input;
    list<target *> Output;
    list<string> Args;
    string Program;
    list<target *> Depends;

    bool NeedsRebuild;
    u64 CommandDepth;

    u64 VisitCount;
};

struct string_arg {
    string Data;
    string_arg() = default;
    string_arg(const char* String) : Data(CreateString((char *)String)) {}
    string_arg(string String) : Data(String) {}
};

struct target_args {
    string_arg Path;
    list<target *> Input;
    list<target *> Output;
    list<string> Args;
    string_arg Program;
    list<target *> Depends;
};

struct state {
    target Targets[10000];
    u64 TargetCount;
    bool ForceRebuild;
    string Errors[10000];
    u64 ErrorCount;
    string ProjectRoot;
    target *CommandsToRun[10000];
    u64 CommandsToRunCount;
};

state State = {};
target NullTarget = {};

target *Target(target Value = {}) {
    target *Result = &State.Targets[State.TargetCount++];
    *Result = Value;
    if (Result->Name.Length == 0) {
        Result->Name = Result->Path;
    }

    // TODO: Maybe don't do this and just force users to do it?
    if (Result->Program.Length > 0 && Result->Output.Count == 0) {
        Result->NeedsRebuild = true;
    }

    for (u64 i = 0; i < Result->Output.Count; i += 1) {
        //TODO: Dedupe with things in depends or things already in input? Or rely on caller to do this.
        assert(Result->Program.Length);
        target *Output = Result->Output.Data[i];
        assert(!Output->ParentCommand);
        Output->ParentCommand = Result;
    }
    return Result;
}

target *FindTarget(string Name) {
    target *Result = &NullTarget;

    for (u64 TargetIndex = 0; TargetIndex < State.TargetCount; TargetIndex += 1) {
        target *T = &State.Targets[TargetIndex];
        if (StringMatch(T->Name, Name)) {
            Result = T;
            break;
        }
    }

    return Result;
}

string TargetString(target *Target) {
    assert(Target->Path.Length);
    string String = Target->Path;
    return String;
}

string TargetListString(list<target *> *List) {
    s_list StringList = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringListAppend(&StringList, " ");
        }

        string String = TargetString(List->Data[i]);
        StringListAppend(&StringList, String);
    }
    return StringListJoin(&StringList);
}

template<typename t>
void ListEnsureCapacity(list<t> *List, u64 NewCount) {
    if (NewCount > List->Capacity) {
        List->Capacity = NewCount * 2;
        if (List->Capacity < 16) {
            List->Capacity = 16;
        }
        List->Data = (t *)realloc(List->Data, sizeof(t) * List->Capacity);
    }
}

template<typename t>
void ListAdd(list<t> *List, t Value) {
    ListEnsureCapacity(List, List->Count + 1);
    List->Data[List->Count++] = Value;
}

void ListAdd(list<target *> *List, const char *String) {
    target *T = Target({ .Path = CreateString((char *)String) });
    ListAdd(List, T);
}
void ListAdd(list<target *> *List, string String) {
    target *T = Target({ .Path = String });
    ListAdd(List, T);
}
void ListAdd(list<target *> *List, target *Target) {
    // TODO: this might be a recursive call
    if (Target->Output.Count) {
        ListEnsureCapacity(List, List->Count + Target->Output.Count);
        for (u64 i = 0; i < Target->Output.Count; i += 1) {
            List->Data[List->Count++] = Target->Output.Data[i];
        }
    }
    else {
        ListEnsureCapacity(List, List->Count + 1);
        List->Data[List->Count++] = Target;
    }
}
void ListAdd(list<target *> *List, list<target *> &Other) {
    for (u64 i = 0; i < Other.Count; i += 1) {
        ListAdd(List, Other.Data[i]);
    }
}
void ListAdd(list<string> *List, const char *String) {
    ListAdd(List, CreateString((char *)String));
}
//void ListAdd(list<string> *List, list<string> StringList) {
//    for (u64 i = 0; i < StringList.Count; i += 1) {
//        ListAdd(List, StringList.Data[i]);
//    }
//}

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

string ArgsToString(list<string> *List) {
    s_list StringList = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringListAppend(&StringList, " ");
        }

        string String = List->Data[i];
        StringListAppend(&StringList, String);
    }
    return StringListJoin(&StringList);
}

string ProjectPath(string Path) {
    return StringConcat(State.ProjectRoot, Path);
}

struct var_parse_result {
    string Variable;
    u64 Index;
    bool HasIndex;
    bool Error;
};

var_parse_result ParseVariable(string String) {
    var_parse_result Result = {};

    // If target list contains multiple items makes makes multiple arguments and quotes each one
    // if the variable is the entirety of
    cut_result Cut = StringCut(String, Strlit("["));
    if (Cut.Found) {
        cut_result IndexPart = StringCut(Cut.After, Strlit("]"));
        if (IndexPart.Found) {
            parsed_num ParsedIndex = StringParseNum(IndexPart.Before);
            if (ParsedIndex.Error) {
                PushError("Error: invalid subscript for variable '%.*s'\n", StrArg(String));
                Result.Error = true;
            }
            else {
                Result.HasIndex = true;
                Result.Index = ParsedIndex.Value;
            }
        }
        else {
            PushError("Error: invalid subscript for variable '%.*s'\n", StrArg(String));
            Result.Error = true;
        }
    }
    Result.Variable = Cut.Before;

    return Result;
}

struct buld_arg_iter {
    string String;
    u64 At;
};

struct buld_arg_iter_item {
    string String;
    bool IsVariable;
    bool Done;
    bool IsEntireArg;
};

// We force any opened and closed braces to contain variables, otherwise braces must be escaped
buld_arg_iter_item BuldArgIterNext(buld_arg_iter *Iter) {
    buld_arg_iter_item Item = {};

    // 01234{INPUT}ABC

    if (Iter->At == Iter->String.Length) {
        return {
            .Done = true,
        };
    }

    bool IsStart = Iter->At == 0;
    u64 Start = Iter->At;
    u64 End = Iter->String.Length;
    bool IsVariable = false;

    for (; Iter->At < Iter->String.Length; Iter->At += 1) {
        u8 c = Iter->String.Str[Iter->At];
        if (c == '{') {
            u64 CloseBraceIndex = StringFindChar(Iter->String, '}', Iter->At + 1);
            if (CloseBraceIndex < Iter->String.Length) {
                if (Iter->At == Start) {
                    Start = Iter->At + 1;
                    End = CloseBraceIndex;
                    IsVariable = true;
                    Iter->At = CloseBraceIndex + 1;
                }
                else {
                    End = Iter->At;
                }
            }
            else {
                Iter->At = Iter->String.Length;
            }
            break;
        }
    }

    return {
        .String = SubstrRange(Iter->String, Start, End),
        .IsVariable = IsVariable,
        .IsEntireArg = IsStart && (Iter->At == Iter->String.Length),
    };
}

bool ProcessArg(list<string> *ArgList, string Arg, list<target *> *Input, list<target *> *Output) {
    bool Ok = true;

    // "{OUTPUT}" -> {'arg1', 'arg2', 'arg3'}
    // "{OUTPUT}{INPUT}" -> {'arg1 arg2 arg3'}
    // "--file={OUTPUT}" -> {'--file=arg1 arg2 arg3'}

    buld_arg_iter Iter = { .String = Arg };

    s_list Builder = {};
    for (buld_arg_iter_item Item = BuldArgIterNext(&Iter); !Item.Done; Item = BuldArgIterNext(&Iter)) {
        if (Item.IsVariable) {
            var_parse_result ParseResult = ParseVariable(Item.String);
            if (ParseResult.Error) {
                Ok = false;
                break;
            }

            string MatchVar = {};
            list<target *> *MatchList = 0;
            if (StringMatch(ParseResult.Variable, Strlit("INPUT"))) {
                MatchList = Input;
            }
            else if (StringMatch(ParseResult.Variable, Strlit("OUTPUT"))) {
                MatchList = Output;
            }
            else {
                PushError("Error: Undefined variable '%.*s'\n", StrArg(ParseResult.Variable));
                Ok = false;
                break;
            }

            if (ParseResult.HasIndex) {
                if (ParseResult.Index >= MatchList->Count) {
                    PushError("Error: variable '%.*s' subscript exceeds list count %lu.\n", StrArg(Item.String), MatchList->Count);
                    Ok = false;
                    break;
                }

                string Replacement = TargetString(MatchList->Data[ParseResult.Index]);
                if (Item.IsEntireArg) {
                    ListAdd(ArgList, Replacement);
                }
                else {
                    StringListAppend(&Builder, Replacement);
                }
            }
            else {
                for (u64 MatchListIndex = 0; MatchListIndex < MatchList->Count; MatchListIndex += 1) {
                    string Replacement = TargetString(MatchList->Data[MatchListIndex]);
                    if (Item.IsEntireArg) {
                        ListAdd(ArgList, Replacement);
                    }
                    else {
                        if (MatchListIndex > 0) {
                            StringListAppend(&Builder, Strlit(" "));
                        }
                        StringListAppend(&Builder, Replacement);
                    }
                }
            }
        }
        else {
            if (Item.IsEntireArg) {
                ListAdd(ArgList, Item.String);
            }
            else {
                StringListAppend(&Builder, Item.String);
            }
        }
    }

    string JoinedArg = StringListJoin(&Builder);
    if (JoinedArg.Length > 0) {
        ListAdd(ArgList, JoinedArg);
    }

    return Ok;
}

struct build_result {
    bool Error;
    bool NeedsRebuild;
};

build_result BuildTarget(target *Target) {
    build_result BuildResult = {
        .NeedsRebuild = Target->NeedsRebuild,
    };
    u64 NewestInputTimestamp = 0;

    Target->VisitCount += 1;

    if (Target->ParentCommand) {
        assert(Target->Input.Count == 0);
        BuildResult = BuildTarget(Target->ParentCommand);
    }
    else {
        for (u64 TargetIndex = 0; TargetIndex < Target->Input.Count; TargetIndex += 1) {
            target *Input = Target->Input.Data[TargetIndex];
            build_result InputBuildResult = BuildTarget(Input);
            BuildResult.Error |= InputBuildResult.Error;
            BuildResult.NeedsRebuild |= InputBuildResult.NeedsRebuild;
            NewestInputTimestamp = Max(NewestInputTimestamp, Input->FileInfo.ModTime);
        }
        for (u64 TargetIndex = 0; TargetIndex < Target->Depends.Count; TargetIndex += 1) {
            target *Dep = Target->Depends.Data[TargetIndex];
            build_result DependsBuildResult = BuildTarget(Dep);
            BuildResult.Error |= DependsBuildResult.Error;
            BuildResult.NeedsRebuild |= DependsBuildResult.NeedsRebuild;
            NewestInputTimestamp = Max(NewestInputTimestamp, Dep->FileInfo.ModTime);
        }
    }

    if (BuildResult.Error) {
        return BuildResult;
    }

    if (Target->Program.Length) {
        if (!BuildResult.NeedsRebuild) {
            for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
                target *Output = Target->Output.Data[TargetIndex];
                assert(Output->Path.Length);
                os_file_info FileInfo = OS_GetFileInfo(ProjectPath(Output->Path));
                assert(FileInfo.Ok); // TODO:
                u64 NewestOutputTimestamp = 0;
                if (FileInfo.Exists) {
                    NewestOutputTimestamp = Max(NewestOutputTimestamp, FileInfo.ModTime);
                }
                Output->FileInfo = FileInfo;
                Output->VisitCount += 1;

                if (!FileInfo.Exists || NewestInputTimestamp > NewestOutputTimestamp) {
                    assert(!Target->Path.Length);
                    assert(Target->Output.Count);
                    assert(Target->Program.Length);
                    BuildResult.NeedsRebuild = true;
                    break;
                }
            }
        }

        if (BuildResult.NeedsRebuild) {
            State.CommandsToRun[State.CommandsToRunCount++] = Target;

            for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
                target *Output = Target->Output.Data[TargetIndex];
                assert(Output->Path.Length);

                // We have to also set NeedsRebuild here otherwise root level commands outputs wont be set
                Output->NeedsRebuild = BuildResult.NeedsRebuild;
            }

        }
    }
    else {
        assert(Target->Path.Length);
        if (!Target->ParentCommand) {
            // Leaf node
            assert(Target->Input.Count == 0);

            os_file_info FileInfo = OS_GetFileInfo(ProjectPath(Target->Path));
            assert(FileInfo.Ok); // TODO:
            if (!FileInfo.Exists) {
                PushError("File '%.*s' does not exist\n", StrArg(Target->Path));
                BuildResult.Error = true;
            }
            Target->FileInfo = FileInfo;
        }
    }

    Target->NeedsRebuild = BuildResult.NeedsRebuild;

    return BuildResult;
}

s32 RunBuildCommands(bool DryRun)
{
    s32 ReturnCode = 0;

    if (State.CommandsToRunCount == 0) {
        printf("Nothing to rebuild.\n");
    }

    for (u64 TargetIndex = 0; TargetIndex < State.CommandsToRunCount; TargetIndex += 1) {
        target *Target = State.CommandsToRun[TargetIndex];

        list<string> SubstitutedArgList = {};

        // TODO: Maybe do this in BuildTarget?
        list<string> *ArgList = &Target->Args;
        for (u64 ArgIndex = 0; ArgIndex < ArgList->Count; ArgIndex += 1) {
            string Arg = ArgList->Data[ArgIndex];
            bool Ok = ProcessArg(&SubstitutedArgList, Arg, &Target->Input, &Target->Output);
            if (Ok) {
                //SubstitutedArgList
            }
            else {
                ReturnCode = 1;
                break;
            }
        }

        if (ReturnCode == 1) {
            break;
        }

        string ArgStr = ArgsToString(&SubstitutedArgList);
        printf("[%lu/%lu] %.*s %.*s\n", TargetIndex+1, State.CommandsToRunCount, StrArg(Target->Program), StrArg(ArgStr));

        assert(Target->Program.Length);

        bool Error = false;

        if (DryRun) {
            for (u64 ArgIndex = 0; ArgIndex < SubstitutedArgList.Count; ArgIndex += 1) {
                string Arg = SubstitutedArgList.Data[ArgIndex];
                printf("'%.*s' ", StrArg(Arg));
            }
            printf("\n");
        }
        else {
            ReturnCode = OS_RunProcess(Target->Program, SubstitutedArgList.Data, SubstitutedArgList.Count);
            if (ReturnCode == 0) {
                for (u64 OutputIndex = 0; OutputIndex < Target->Output.Count; OutputIndex += 1) {
                    target *Output = Target->Output.Data[OutputIndex];
                    assert(Output->Path.Length);
                    os_file_info FileInfo = OS_GetFileInfo(ProjectPath(Output->Path));
                    if (!FileInfo.Exists) {
                        PushError("File '%.*s' does not exist after build command\n", Target->Path);
                        ReturnCode = 1;
                        Error = true;
                    }
                    Output->FileInfo = FileInfo;
                }
            }
        }

        if (Error) {
            break;
        }
    }

    return ReturnCode;
}
