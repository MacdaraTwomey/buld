
#include "buld.h"

target *Target(target Value = {}) {
    target *Result = &State.Targets[State.TargetCount++];
    *Result = Value;
    for (u64 i = 0; i < Result->Output.Count; i += 1) {
        assert(Result->Program);
        target *Output = Result->Output.Elements[i];
        assert(!Output->ParentCommand);
        Output->ParentCommand = Result;
    }
    return Result;
}

void ListAdd(target_list *List, const char *String) {
    List->Elements[List->Count++] = Target({ .Path = String });
}
void ListAdd(target_list *List, target *Target) {
    if (Target->Output.Count) {
        for (u64 i = 0; i < Target->Output.Count; i += 1) {
            List->Elements[List->Count++] = Target->Output.Elements[i];
        }
    }
    else {
        List->Elements[List->Count++] = Target;
    }
}

target_list::target_list(const char *String) {
    Count = 0;
    Elements = PushArray(1, target *);
    ListAdd(this, String);
}
target_list::target_list(target *Target) {
    Count = 0;
    if (Target->Output.Count) {
        Elements = PushArray(Target->Output.Count, target *);
    }
    else {
        Elements = PushArray(1, target *);
    }
    ListAdd(this, Target);
}

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

        target *Target = List->Elements[i];
        assert(Target->Path);
        string String = CreateString((char *)Target->Path);
        StringListAppend(&StringList, String);
    }
    return StringListJoin(&StringList);
}

string ProjectPath(string Path) {
    return StringConcat(State.ProjectRoot, Path);
}

struct build_result {
    bool Error;
    s32 ReturnCode;
};

build_result BuildTarget(target *Target) {
    build_result BuildResult = {};
    u64 NewestInputTimestamp = 0;

    if (Target->ParentCommand) {
        assert(Target->Input.Count == 0);
        BuildResult = BuildTarget(Target->ParentCommand);
    }
    else {
        for (u64 TargetIndex = 0; TargetIndex < Target->Input.Count; TargetIndex += 1) {
            target *Input = Target->Input.Elements[TargetIndex];
            build_result InputBuildResult = BuildTarget(Input);
            if (InputBuildResult.Error) {
                BuildResult = InputBuildResult;
                break;
            }
            NewestInputTimestamp = Max(NewestInputTimestamp, Input->FileInfo.ModTime);
        }
    }

    if (BuildResult.Error) {
        return BuildResult;
    }

    u64 NewestOutputTimestamp = 0;
    for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
        target *Output = Target->Output.Elements[TargetIndex];
        assert(Output->Path);
        os_file_info FileInfo = OS_GetFileInfo(ProjectPath(CreateString((char *)Output->Path)));
        if (FileInfo.Exists) {
            NewestOutputTimestamp = Max(NewestOutputTimestamp, Output->FileInfo.ModTime);
        }
        else {
            NewestOutputTimestamp = UINT64_MAX;
        }
        Output->FileInfo = FileInfo;
    }

    if (NewestOutputTimestamp > NewestInputTimestamp) {
        string Command = CreateString((char *)Target->Args);
        Command = FindReplace(Command, Strlit("{INPUT}"), TargetListString(&Target->Input));
        Command = FindReplace(Command, Strlit("{OUTPUT}"), TargetListString(&Target->Output));
        string Program = CreateString((char *)Target->Program);

        printf("%.*s %.*s\n", StrArg(Program), StrArg(Command));

        string_list Args = {};
        arg_parser Parser = { .String = Command };

        while (!Parser.Error) {
            string Arg = GetArg(&Parser);
            if (Arg.Length == 0) {
                break;
            }

            ArrayAdd(&Args, Arg);
        }

        if (Parser.Error) {
            PushError("Error parsing command %.*s\n", Strval(Command));
            BuildResult.Error = true;
        }

        s32 ReturnCode = OS_RunProcess(Program, Args.Data, Args.Count);
        if (ReturnCode == 0) {
            for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
                target *Output = Target->Output.Elements[TargetIndex];
                assert(Output->Path);
                os_file_info FileInfo = OS_GetFileInfo(ProjectPath(CreateString((char *)Output->Path)));
                if (!FileInfo.Exists) {
                    PushError("File '%.*s' does not exist after build command\n", Target->Path);
                    BuildResult.Error = true;
                }
                Output->FileInfo = FileInfo;
            }
        }
        else {
            BuildResult.Error = true;
            BuildResult.ReturnCode = ReturnCode;
        }
    }
    else {
        if (Target->Input.Count == 0) {
            assert(Target->Path);
            os_file_info FileInfo = OS_GetFileInfo(ProjectPath(CreateString((char *)Target->Path)));
            if (!FileInfo.Exists) {
                // Leaf node
                PushError("File '%.*s' does not exist\n", StrArg(CreateString((char *)Target->Path)));
                BuildResult.Error = true;
            }
            Target->FileInfo = FileInfo;
        }
    }

    return BuildResult;
}

void WriteGraphNode(FILE *File, target *Target) {
    if (Target->ParentCommand) {
        WriteGraphNode(File, Target->ParentCommand);
    }
    else {
        for (u64 TargetIndex = 0; TargetIndex < Target->Input.Count; TargetIndex += 1) {
            target *Input = Target->Input.Elements[TargetIndex];
            WriteGraphNode(File, Input);
            fprintf(File, "x%p -> x%p\n", (void *)Input, (void *)Target);
        }
    }

    if (Target->Program) {
        fprintf(File, "x%p [label=\"%.*s %.*s\", shape=box]\n", (void *)Target, StrArg(CreateString((char *)Target->Program)), StrArg(CreateString((char *)Target->Args)));
    }
    else if (Target->Path) {
        fprintf(File, "x%p [label=\"%.*s\", shape=ellipse]\n", (void *)Target, StrArg(CreateString((char *)Target->Path)));
    }
    else {
        assert(0);
    }


    for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
        target *Output = Target->Output.Elements[TargetIndex];
        fprintf(File, "x%p -> x%p\n", (void *)Target, (void *)Output);
        // Print this (sometimes again) because the main programs outputs wont get labels otherwise
        fprintf(File, "x%p [label=\"%.*s\", shape=ellipse]\n", (void *)Output, StrArg(CreateString((char *)Output->Path)));
    }
}

void WriteGraph(FILE *File, target *Target) {
    // strict makes it so two identical edges are only drawn once
    fprintf(File, "strict digraph G {\n");
    //fprintf(File, "rankdir=BT;\n");
    //fprintf(File, "subgraph code_block {\n");
    WriteGraphNode(File, Target);
    //fprintf(File, "}\n");
    fprintf(File, "}\n");
}

int main(int ArgCount, char **Args) {
    string GraphPath = {};
    for (s32 ArgIndex = 1; ArgIndex < ArgCount; ArgIndex += 1) {
        string Arg = CreateString(Args[ArgIndex]);
        if (StringMatch(Arg, Strlit("--graph"))) {
            ArgIndex += 1;
            if (ArgIndex < ArgCount) {
                GraphPath = CreateString(Args[ArgIndex]);
            }
        }
    }

    // Ideas:
    // * PushFlags()
    // * PushInclude()
    // * get list of object that need to be rebuilt.
    //   maybe a different data structure that commands + files
    // * get list of commands to run, in DAG
    // * Get true source -> target list
    // * Maybe Dir("bin/") to push and pop build dir


    State.ProjectRoot = Strlit("/home/mac/projects/buld/");

    //target *GenSrc = Target({
    //    .Input = "sample/gen-src.txt",
    //    .Output = List("sample/build/gen-src.cpp", "sample/build/gen-src.h"),
    //    .Args = "--do-gen",
    //    .Program = "gener",
    //});
    //target *GenObj = Target({
    //    .Input = GenSrc, // GenSrc->Output.Elements[0],
    //    .Output = "sample/build/gen-src.o",
    //    .Args = "-c {INPUT} -o {OUTPUT} -g",
    //    .Program = "compiler",
    //});



    target *Main = Target({
        .Input = "sample/main.cpp",
        .Output = "sample/build/main.o",
        .Args = "-c {INPUT} -o {OUTPUT} -g",
        .Program = "clang++",
    });

    target *File1 = Target({
        .Input = "sample/file1.cpp",
        .Output = "sample/build/file1.o",
        .Args = "-c {INPUT} -o {OUTPUT} -g",
        .Program = "clang++",
    });

    target *Exe = Target({
        .Input = List(Main, File1),
        .Output = "sample/build/main.exe",
        .Args = "-o {OUTPUT} {INPUT}",
        .Program = "clang++",
    });

    s32 ReturnCode = 0;

    if (GraphPath.Length) {
        // TODO: No fopen and real string handling
        FILE *File = fopen((char *)GraphPath.Str, "wb");
        if (File) {
            WriteGraph(File, Exe);
        }
        else {
            assert(0);
        }
    }
    else {
        build_result BuildResult = BuildTarget(Exe);
        if (BuildResult.Error) {
            ReturnCode = BuildResult.ReturnCode;
        }
    }

    for (u64 ErrorIndex = 0; ErrorIndex < State.ErrorCount; ErrorIndex += 1) {
        printf("%.*s\n", StrArg(State.Errors[ErrorIndex]));
        ReturnCode = 1;
    }

    return ReturnCode;
}

