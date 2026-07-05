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

#if defined(_WIN32)
#  define OS_WINDOWS 1
#elif defined(__linux__)
#  define OS_LINUX 1
#elif defined(__APPLE__) || defined(__MACH__)
#  define OS_MAC 1
#elif defined(__unix__)
#  define OS_UNIX 1
#else
#  error "Could not detect operating system"
#endif

#if !defined(OS_WINDOWS)
#  define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
#  define OS_LINUX 0
#endif
#if !defined(OS_MAC)
#  define OS_MAC 0
#endif
#if !defined(OS_UNIX)
#  define OS_UNIX 0
#endif

// Check clang first because clang often will define __GNUC__ or _MSC_VER as well
#if defined(__clang__)
#  define COMPILER_CLANG 1
#elif defined(_MSC_VER)
#  define COMPILER_MSVC 1
#elif defined(__GNUC__)
#  define COMPILER_GCC 1
#else
#  error "Could not detect compiler"
#endif

#if !defined(COMPILER_CLANG)
#  define COMPILER_CLANG 0
#endif
#if !defined(COMPILER_MSVC)
#  define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_GCC)
#  define COMPILER_GCC 0
#endif

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

#if COMPILER_CLANG || COMPILER_GCC
// 1 based indexing for parameter
#  define FORMAT_STRING_CHECK(StringIndex, ArgsIndex)  __attribute__((__format__ (__printf__, StringIndex, ArgsIndex)))
#else
#  define FORMAT_STRING_CHECK(StringIndex, ArgsIndex)
#endif

struct string {
    u64 Length;
    u8 *Str;
};

#define StrArg(String) ((int)((String).Length)), ((String).Str)
#define Str(String) (string{ sizeof(String)-1, (u8 *)String })

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
            .After = Str(""),
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
string PushStringfArgs(const char *Format, va_list Args)
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


string PushStringf(const char *Format, ...) FORMAT_STRING_CHECK(1, 2);

string PushStringf(const char *Format, ...)
{
    va_list Args;
    va_start (Args, Format);
    string Result = PushStringfArgs(Format, Args);
    va_end (Args);

    return Result;
}

struct string_builder {
    string String;
};

void StringBuilderAppend(string_builder *Builder, string String) {
    u64 NewLength = Builder->String.Length + String.Length;
    u8 *Memory = Builder->String.Str;
    u8 *NewMemory = (u8 *)realloc(Memory, NewLength);
    memcpy(NewMemory + Builder->String.Length, String.Str, String.Length);
    Builder->String = CreateString(NewMemory, NewLength);
}

void StringBuilderAppend(string_builder *Builder, const char *String) {
    StringBuilderAppend(Builder, CreateString((u8 *)String));
}

string FindReplace(string String, string MatchString, string ReplaceString) {
    u64 Index = StringFindStr(String, MatchString);
    string Before = StringPrefix(String, Index);
    string After = StringSkip(String, Index + MatchString.Length);
    string Result = StringConcat(Before, ReplaceString, After);
    return Result;
}

char *ZString(string String) {
    char *Buffer = PushArray(String.Length, char);
    memcpy(Buffer, String.Str, String.Length);
    return Buffer;
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

struct os_directory_iterator {
    void *Handle;
    bool Error;
    bool Done;
};

struct os_filesystem_entry {
    string Name;
    os_file_type Type;
};

struct os_wildcard_file_iter {
    glob_t Glob;
    u64 At;
    bool Done;
    bool Error;
};

enum os_file_mode {
    OS_FileMode_None   = 0,

    //                   Octal values
    OS_FileMode_UserR  = 0400,
    OS_FileMode_UserW  = 0200,
    OS_FileMode_UserX  = 0100,
    OS_FileMode_GroupR = 0040,
    OS_FileMode_GroupW = 0020,
    OS_FileMode_GroupX = 0010,
    OS_FileMode_OtherR = 0004,
    OS_FileMode_OtherW = 0002,
    OS_FileMode_OtherX = 0001,

    OS_FileMode_UserRW   = OS_FileMode_UserR|OS_FileMode_UserW,
    OS_FileMode_UserRX   = OS_FileMode_UserR|OS_FileMode_UserX,
    OS_FileMode_UserRWX  = OS_FileMode_UserR|OS_FileMode_UserW|OS_FileMode_UserX,

    OS_FileMode_GroupRW  = OS_FileMode_GroupR|OS_FileMode_GroupW,
    OS_FileMode_GroupRX  = OS_FileMode_GroupR|OS_FileMode_GroupX,
    OS_FileMode_GroupRWX = OS_FileMode_GroupR|OS_FileMode_GroupW|OS_FileMode_GroupX,

    OS_FileMode_OtherRW  = OS_FileMode_OtherR|OS_FileMode_OtherW,
    OS_FileMode_OtherRX  = OS_FileMode_OtherR|OS_FileMode_OtherX,
    OS_FileMode_OtherRWX = OS_FileMode_OtherR|OS_FileMode_OtherW|OS_FileMode_OtherX,

    OS_FileMode_DefaultDir  = OS_FileMode_UserRWX|OS_FileMode_GroupRX|OS_FileMode_OtherRX,
    OS_FileMode_DefaultFile = OS_FileMode_UserRWX|OS_FileMode_GroupR|OS_FileMode_OtherR,
    OS_FileMode_DefaultExe  = OS_FileMode_UserRWX|OS_FileMode_GroupRX|OS_FileMode_OtherRX,
};

enum os_create_directory_flags {
    OS_CreateDirectoryFlags_None = 0,
    OS_CreateDirectoryFlags_AllowExists = 1,
};

#if OS_LINUX

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

os_directory_iterator OS_DirectoryIterator(string DirectoryPath) {

    char *ZDirectoryPath = ZString(DirectoryPath);

    DIR *Dir = opendir(ZDirectoryPath);
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

os_wildcard_file_iter OS_WildcardFileIter(string Pattern) {
    char *ZPattern = ZString(Pattern);

    glob_t Glob = {};
    int Flags = GLOB_MARK; // add '/' to dirs
    int Result = glob(ZPattern, Flags, NULL, &Glob);

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

struct os_read_entire_file_result {
    string String;
    bool Error;
    bool NotExist;
};

os_read_entire_file_result OS_ReadEntireFile(string Path) {
    os_read_entire_file_result Result = {};

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

bool OS_CreateDirectory(string Path, os_file_mode Mode, os_create_directory_flags Flags) {
    bool Ok = true;

    char *ZPath = ZString(Path);

    int Result = mkdir(ZPath, (mode_t)Mode);
    if (Result != 0) {
        if (errno == EEXIST && Flags == OS_CreateDirectoryFlags_AllowExists) {
        }
        else {
            fprintf(stderr, "Failed to create directory '%.*s'. %s", StrArg(Path), strerror(errno));
            Ok = false;
        }
    }

    return Ok;
}

bool OS_ReplaceCurrentProcess(string ProgramPath, char **Argv) {
    char *ZProgramPath = ZString(ProgramPath);

    int ExecResult = execv(ZProgramPath, Argv);
    if (ExecResult < 0) {
        fprintf(stdout, "Failed to exec %.*s. %s", StrArg(ProgramPath), strerror(errno));
        return false;
    }

    return true;
}

s32 OS_RunProcess(string Program, string *Args, s32 ArgCount) {
    s32 ReturnCode = -1;

    u64 ArgvIndex = 0;
    char **Argv = PushArray(ArgCount + 2, char *);

    char *ZProgram = ZString(Program);

    Argv[ArgvIndex++] = ZProgram;

    for (s32 i = 0; i < ArgCount; i += 1) {
        Argv[ArgvIndex++] = ZString(Args[i]);
    }

    Argv[ArgvIndex++] = 0;

    pid_t PID = fork();
    if (PID == -1) {
    }
    else if (PID == 0) {
        if (execvp(ZProgram, Argv) < 0) {
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

    const char *SymlinkToProgram = "/proc/self/exe";

    // Allocates buffer that must be freed for you
    char *ResolvedPath = realpath(SymlinkToProgram, 0);
    if (ResolvedPath) {
        ExecutablePath = CreateString(ResolvedPath);
    }
    else {
        fprintf(stdout, "Unable to resolve %s symlink. %s\n", SymlinkToProgram, strerror(errno));
    }

    return ExecutablePath;
}

void OS_Exit(s32 ReturnCode) {
    exit(ReturnCode);
}

#else
#  error "Only Linux OS is implemented currently."
#endif // OS_LINUX

//-----------------------------------------------------------------------------
// build library
//

struct target;

template<typename t>
struct list;

template <typename T>
struct is_list : false_type {};

template <typename T>
struct is_list<list<T>> : true_type {};

template<typename t>
void ListAdd(list<t> *List, t Value);

template<typename t>
struct list {
    t *Data;
    u64 Count;
    u64 Capacity;

    list() = default;

    template<typename... args>
    list(args&&... Args) : Data(0), Count(0), Capacity(0) {

        auto ProcessItem = [&]<typename T>(T&& Item) {
            if constexpr (is_list<remove_reference_t<T>>::value) {
                for (u64 i = 0; i < Item.Count; i += 1) {
                    ListAdd(this, Item.Data[i]);
                }
            }
            else {
                ListAdd(this, Item);
            }
        };

        (ProcessItem(forward<args>(Args)), ...);
    }

    template<typename T>
    list operator+(const T& Right) {
        list List = {};

        for (u64 i = 0; i < this->Count; i += 1) {
            ListAdd(&List, this->Data[i]);
        }
        ListAdd(&List, Right);

        return List;
    }

    template<typename T>
    list &operator+=(const T& Right) {
        ListAdd(this, Right);
        return *this;
    }
};

typedef s32 (*command_proc)(target *Target);

struct target {
    string Name;
    string Path;
    os_file_info FileInfo;
    target *ParentCommand;

    list<target *> Input;
    list<target *> Output;
    list<string> Args;
    string Program;
    // Run after Program if Program is set
    command_proc CommandProc;

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
    command_proc CommandProc;
    list<target *> Depends;
};

struct state {
    target Targets[10000];
    u64 TargetCount;
    string Errors[10000];
    u64 ErrorCount;
    string ProjectRoot;
    target *CommandsToRun[10000];
    u64 CommandsToRunCount;

    bool ForceRebuild;
    bool DryRun;
};

state State = {};
target NullTarget = {};


bool IsCommandTarget(target *Target) {
    return Target->Program.Length || Target->CommandProc;
}

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
        assert(IsCommandTarget(Result));
        //TODO: Dedupe with things in depends or things already in input? Or rely on caller to do this.
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
    string_builder Builder = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringBuilderAppend(&Builder, " ");
        }

        string String = TargetString(List->Data[i]);
        StringBuilderAppend(&Builder, String);
    }
    return Builder.String;
}

string ArgsToString(list<string> *List) {
    string_builder Builder = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringBuilderAppend(&Builder, " ");
        }

        string String = List->Data[i];
        StringBuilderAppend(&Builder, String);
    }
    return Builder.String;
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


// This causes infinte recursive of ListAdd parameter pack function
//void ListAdd(list<string> *List, list<string> StringList) {
//    for (u64 i = 0; i < StringList.Count; i += 1) {
//        ListAdd(List, StringList.Data[i]);
//    }
//}

void PushError(string String) {
    State.Errors[State.ErrorCount++] = String;
}

void PushError(const char *Format, ...) FORMAT_STRING_CHECK(1, 2);

void PushError(const char *Format, ...) {
    va_list Args;
    va_start (Args, Format);
    string Result = PushStringfArgs(Format, Args);
    va_end (Args);
    PushError(Result);
}

string ProjectPath(string Path) {
    string Result = Path;
    if (Path.Length > 0 && Path.Str[0] != '/') {
        Result = StringConcat(State.ProjectRoot, Path);
    }

    return Result;
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
    cut_result Cut = StringCut(String, Str("["));
    if (Cut.Found) {
        cut_result IndexPart = StringCut(Cut.After, Str("]"));
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

    string_builder Builder = {};
    for (buld_arg_iter_item Item = BuldArgIterNext(&Iter); !Item.Done; Item = BuldArgIterNext(&Iter)) {
        if (Item.IsVariable) {
            var_parse_result ParseResult = ParseVariable(Item.String);
            if (ParseResult.Error) {
                Ok = false;
                break;
            }

            string MatchVar = {};
            list<target *> *MatchList = 0;
            if (StringMatch(ParseResult.Variable, Str("INPUT"))) {
                MatchList = Input;
            }
            else if (StringMatch(ParseResult.Variable, Str("OUTPUT"))) {
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
                    StringBuilderAppend(&Builder, Replacement);
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
                            StringBuilderAppend(&Builder, Str(" "));
                        }
                        StringBuilderAppend(&Builder, Replacement);
                    }
                }
            }
        }
        else {
            if (Item.IsEntireArg) {
                ListAdd(ArgList, Item.String);
            }
            else {
                StringBuilderAppend(&Builder, Item.String);
            }
        }
    }

    string JoinedArg = Builder.String;
    if (JoinedArg.Length > 0) {
        ListAdd(ArgList, JoinedArg);
    }

    return Ok;
}


string UnEscapeMakeDependencyPath(string String) {
    string Unescaped = {};

    // Things clang does
    // '\' -> '/' I think it does this for windows backslash reasons, but it is ambiguous, therefore ignore this issue
    // '$'  -> '$$'
    // '#'  -> '\#'
    // ' '  -> '\ '
    // Doesn't escape newlines, other whitespace, colons or other special characters.

    if ((StringFindChar(String, '\\') < String.Length) ||
        (StringFindChar(String, '$') < String.Length)) {
        Unescaped = String;
    }
    else {
        u64 Length = 0;
        u8 *Buffer = PushArray(String.Length, u8);
        for (u64 i = 0; i < String.Length; i += 1) {
            u8 c = String.Str[i];
            if (c == '\\') {
                u8 NextC = StringGetChar(String, i+1);
                if (NextC == ' ' || NextC == '#') {
                    i += 1;
                    Buffer[Length++] = NextC;
                }
                else {
                    Buffer[Length++] = c;
                }
            }
            else if (c == '$' && StringGetChar(String, i+1) == '$') {
                i += 1;
                Buffer[Length++] = c;
            }
            else {
                Buffer[Length++] = c;
            }
        }
        Unescaped = CreateString(Buffer, Length);
    }

    return Unescaped;
}

list<target *> ParseDependencyFile(string String) {

    list<target *> List = {};

    // TODO: should we ensure the target of these matches what we expect

    // You can always search for ': ' to know end of target, because spaces will be escaped
    // Spaces can be used to separate dependencies also
    cut_result Parts = StringCut(String, Str(": "));
    if (Parts.Found) {
        string Target = Parts.Before;
        string Deps = Parts.After;
        if (StringGetChar(Deps, Deps.Length-1) == '\n') {
            Deps.Length -= 1;
        }

        u64 DepStart = 0;
        for (u64 i = 0; i < Deps.Length;) {
            u64 SpaceIndex = StringFindChar(Deps, (u8)' ', i);
            if (((SpaceIndex < Deps.Length) && (StringGetChar(Deps, SpaceIndex-1) != '\\')) || (SpaceIndex == Deps.Length)) {
                string Path = SubstrRange(Deps, DepStart, SpaceIndex);
                string UnescapedPath = UnEscapeMakeDependencyPath(Path);
                //printf("DEP: %.*s: %.*s\n", StrArg(Target), StrArg(UnescapedPath));

                ListAdd(&List, UnescapedPath);

                u64 At = SpaceIndex + 1;
                while (true) {
                    u8 c = StringGetChar(Deps, At);
                    if (c == ' ') {
                        At += 1;
                    }
                    else if (c == '\\' && StringGetChar(Deps, At+1) == '\n') {
                        At += 2;
                    }
                    else {
                        break;
                    }
                }
                DepStart = At;
                i = At;
            }
            else {
                i += 1;
            }
        }
    }

    return List;
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

    if (IsCommandTarget(Target)) {
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

                // We have to also set NeedsRebuild here otherwise root level commands outputs wont be set (do we care?)
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

s32 BuildTargetByName(list<string> *TargetNames) {
    s32 ReturnCode = 0;
    for (u64 TargetIndex = 0; TargetIndex < TargetNames->Count; TargetIndex += 1) {
        string TargetName = TargetNames->Data[TargetIndex];
        target *Target = FindTarget(TargetName);
        if (Target == &NullTarget) {
            fprintf(stderr, "Error: unknown target '%.*s'.\n", StrArg(TargetName));
            ReturnCode = 1;
            break;
        }

        build_result BuildResult = BuildTarget(Target);
        if (BuildResult.Error) {
            ReturnCode = 1;
            break;
        }
    }

    return ReturnCode;
}


s32 RunBuildCommands()
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

        // TODO: What to print if we only have a CommandProc?

        string ArgStr = ArgsToString(&SubstitutedArgList);
        // Only print either the Program or the CommandProc
        if (Target->Program.Length) {
            printf("[%lu/%lu] %.*s %.*s\n", TargetIndex+1, State.CommandsToRunCount, StrArg(Target->Program), StrArg(ArgStr));
        }
        else if (Target->CommandProc) {
            printf("[%lu/%lu] CommandProc", TargetIndex+1, State.CommandsToRunCount);
            if (Target->Input.Count) {
                string InputStr = TargetListString(&Target->Input);
                printf(" Input='%.*s'", StrArg(InputStr));
            }
            if (Target->Output.Count) {
                string OutputStr = TargetListString(&Target->Output);
                printf(" Output='%.*s'", StrArg(OutputStr));
            }
            if (Target->Args.Count) {
                printf(" Args='%.*s'", StrArg(ArgStr));
            }
            printf("\n");
        }

        if (State.DryRun) {
            continue;
        }

        if (Target->Program.Length) {
            ReturnCode = OS_RunProcess(Target->Program, SubstitutedArgList.Data, SubstitutedArgList.Count);
        }
        if (Target->CommandProc) {
            ReturnCode = Target->CommandProc(Target);
        }

        if (ReturnCode == 0) {
            for (u64 OutputIndex = 0; OutputIndex < Target->Output.Count; OutputIndex += 1) {
                target *Output = Target->Output.Data[OutputIndex];
                assert(Output->Path.Length);
                os_file_info FileInfo = OS_GetFileInfo(ProjectPath(Output->Path));
                if (!FileInfo.Exists) {
                    PushError("File '%.*s' does not exist after build command\n", StrArg(Target->Path));
                    ReturnCode = 1;
                }
                Output->FileInfo = FileInfo;
            }
        }

        if (ReturnCode != 0) {
            break;
        }
    }

    return ReturnCode;
}

void RebuildSelf(char **Argv, string SourcePath) {

    string Filename = FilenameFromPath(SourcePath);
    string FilenameNoExt = RemoveExtension(Filename);
    string Dir = DirectoryFromPath(SourcePath);

    string ProgramPath = OS_GetCurrentExecutablePath();

    string DependencyFile = StringConcat(Dir, FilenameNoExt, Str(".d"));
    os_read_entire_file_result ReadResult = OS_ReadEntireFile(DependencyFile);
    if (ReadResult.Error) {
        PushError("Warning: Failed to read %.*s.", StrArg(DependencyFile));
    }

    list<target *> Headers = ParseDependencyFile(ReadResult.String);

    // TODO: Run compiler to get list of headers rather than read .d file?
    // This does force you to run compiler every time.

    string CompilerProgram = {};
    if (COMPILER_CLANG) {
        CompilerProgram = Str("clang++");
    }
    else if (COMPILER_GCC) {
        CompilerProgram = Str("g++");
    }
    else {

    }

    target *BuldProgram = Target({
        .Input = SourcePath,
        .Output = { ProgramPath, DependencyFile },
        .Args = {
            "-g",
            "-std=c++20",
            "-fno-strict-aliasing",
            "{INPUT}",
            "-o", "{OUTPUT[0]}",
            "-MMD", "-MF", "{OUTPUT[1]}"},
        .Program = CompilerProgram,
        .Depends = Headers,
    });

    bool Error = false;

    build_result BuildResult = BuildTarget(BuldProgram);
    if (BuildResult.Error) {
        Error = true;
    }
    else if (BuildResult.NeedsRebuild) {
        fprintf(stdout, "Rebuilding %.*s\n\n", StrArg(SourcePath));

        s32 ReturnCode = RunBuildCommands();
        if (ReturnCode == 0) {
            fprintf(stdout, "\n");
            bool Ok = OS_ReplaceCurrentProcess(ProgramPath, Argv);
            if (!Ok) {
                Error = true;
            }
        }
        else {
            Error = true;
        }
    }

    State = {};

    if (Error) {
        OS_Exit(1);
    }
}