
#include "buld.h"

// main1: main.o file1.o library.dll 
// library: obj1.o obj2.o obj3.o

// Other ideas and features
// way to have read, write, copy, delete file operations that can be done incrementally
// globals aren't that bad for a program like this

template<class T> struct remove_reference { typedef T type; };
template<class T> struct remove_reference<T&> { typedef T type; };
template<class T> struct remove_reference<T&&> { typedef T type; };
template< class T >
using remove_reference_t = typename remove_reference<T>::type;

struct true_type {
    static constexpr bool value = true;
    using value_type = bool;
    using type       = true_type;
    constexpr operator bool() const noexcept { return value; }
};

struct false_type {
    static constexpr bool value = false;
    using value_type = bool;
    using type       = false_type;
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

struct target;

struct target_list {
    target **Elements;
    u64 Count;

    target_list() = default;
    target_list(const char *String);
    target_list(target *Target);
};

struct target {
    string Name;
    target_list Input;
    target_list Output;
    string Command;
    string Program;
};

struct state {
    target Targets[10000];
    u64 TargetCount;
};

state State = {};

#if 0
struct target_list;
enum list_element_kind{
    ListElementKind_Target;
    ListElementKind_String;
};

struct list_element {
    list_element_kind Kind;
    union {
        target *Target;
        char *String;
    };
    list_element(target *Target) : Kind(ListElementKind_Target), Target(Target) {}
    list_element(char *String)   : Kind(ListElementKind_String), String(String) {}
};

struct target_list {
    target *List[100];
    u64 ListCount;
    target_list(target *Target) {
        List[0] = Target;
        ListCount = 1;
    }
    target_list(char *String) {
        List[0] = Target({ .Name = String });
        ListCount = 1;
    }
    target_list(std::initializer_list<list_element> List) {
        List[0] = Target({ .Name = String });
        ListCount = 1;
    }
};
#endif

target *CreateTarget(target Value = {}) {
    target *Result = &State.Targets[State.TargetCount++];
    *Result = Value;
    return Result;
}

#define Target(...) CreateTarget(target{__VA_ARGS__})


target_list::target_list(const char *String) {
    Elements = PushArray(1, target *);
    Count = 0;
    Elements[Count++] = Target( .Name = String );
}
target_list::target_list(target *Target) {
    Elements = PushArray(1, target *);
    Count = 0;
    Elements[Count++] = Target;
}

void ListAdd(target_list *List, const char *String) { List->Elements[List->Count++] = Target( .Name = String ); }
void ListAdd(target_list *List, target *Target)     { List->Elements[List->Count++] = Target; }

template<typename... args>
target_list List(args&&... Args) {
    u64 ArgCount = sizeof...(Args);
    target_list List = {};
    List.Elements = PushArray(ArgCount, target *),
    (ListAdd(&List, forward<args>(Args)), ...);
    return List;
}

string TargetListString(target_list *List) {
    s_list StringList = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringListAppend(&StringList, " ");
        }

        string String = {};
        target *Target = List->Elements[i];
        if (Target->Name.Length == 0) {
            String = TargetListString(&Target->Output);
        }
        else {
            String = Target->Name;
        }
        StringListAppend(&StringList, String);
    }
    return StringListJoin(&StringList);
}

void BuildTarget(target *Target) {
    string Command = Target->Command;
    Command = FindReplace(Command, "{OUTPUT}", TargetListString(&Target->Output));
    Command = FindReplace(Command, "{INPUT}", TargetListString(&Target->Input));
    printf("%.*s\n", StrArg(Command));
}

int main(int ArgCount, char **Args) {

    target *Main = Target(
        .Input = "main.c",
        .Output = "main.o",
        .Command = "-c {INPUT} -o {OUTPUT} -g",
        .Program = "clang",
    );

    target *Obj1 = Target(
        .Input = "obj1.cpp",
        .Output = "obj1.o",
        .Command = "-c {INPUT} -o {OUTPUT} -g",
        .Program = "clang",
    );

   target *Exe = Target(
        .Input = List(Main, Obj1, "another.o"),
        .Output = "exe",
        .Command = "-o {OUTPUT} {INPUT}",
        .Program = "clang",
    );

    BuildTarget(Exe);
}

