
#include "buld.h"

target *Target(target_args Value = {}) {
    target *Result = &State.Targets[State.TargetCount++];
    //*Result = Value;
    *Result = {
        .Path = Value.Path.Data,
        .Input = Value.Input,
        .Output = Value.Output,
        .Args = Value.Args.Data,
        .Program = Value.Program.Data,
        .Depends = Value.Depends,
    };
    for (u64 i = 0; i < Result->Output.Count; i += 1) {
        //TODO: Dedupe with things in depends or things already in input? Or rely on caller to do this.
        assert(Result->Program.Length);
        target *Output = Result->Output.Elements[i];
        assert(!Output->ParentCommand);
        Output->ParentCommand = Result;
    }
    return Result;
}

void ListAdd(target_list *List, const char *String) {
    List->Elements[List->Count++] = Target({ .Path = CreateString((char *)String) });
}
void ListAdd(target_list *List, string String) {
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
    ListAdd(this, CreateString((char *)String));
}
target_list::target_list(string String) {
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

string TargetString(target *Target) {
    assert(Target->Path.Length);
    string String = Target->Path;
    return String;
}

string TargetListString(target_list *List) {
    s_list StringList = {};
    for (u64 i = 0; i < List->Count; i += 1) {
        if (i > 0) {
            StringListAppend(&StringList, " ");
        }

        string String = TargetString(List->Elements[i]);
        StringListAppend(&StringList, String);
    }
    return StringListJoin(&StringList);
}

string ProjectPath(string Path) {
    return StringConcat(State.ProjectRoot, Path);
}

struct build_result {
    bool Error;
    bool NeedsRebuild;
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
            BuildResult.Error |= InputBuildResult.Error;
            BuildResult.NeedsRebuild |= InputBuildResult.NeedsRebuild;
            NewestInputTimestamp = Max(NewestInputTimestamp, Input->FileInfo.ModTime);
        }
        for (u64 TargetIndex = 0; TargetIndex < Target->Depends.Count; TargetIndex += 1) {
            target *Dep = Target->Depends.Elements[TargetIndex];
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
            u64 NewestOutputTimestamp = 0;
            for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
                target *Output = Target->Output.Elements[TargetIndex];
                assert(Output->Path.Length);
                os_file_info FileInfo = OS_GetFileInfo(ProjectPath(Output->Path));
                if (FileInfo.Exists) {
                    NewestOutputTimestamp = Max(NewestOutputTimestamp, FileInfo.ModTime);
                }
                else {
                    NewestOutputTimestamp = UINT64_MAX;
                }
                Output->FileInfo = FileInfo;
            }

            if (NewestInputTimestamp > NewestOutputTimestamp) {
                assert(!Target->Path.Length);
                assert(Target->Output.Count);
                assert(Target->Program.Length);
                BuildResult.NeedsRebuild = true;
            }
        }

        if (BuildResult.NeedsRebuild) {
            State.CommandsToRun[State.CommandsToRunCount++] = Target;

            for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
                target *Output = Target->Output.Elements[TargetIndex];
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

    if (Target->Program.Length) {
        fprintf(File, "x%p [label=\"%.*s %.*s\", shape=box", (void *)Target, StrArg(Target->Program), StrArg(Target->Args));
    }
    else if (Target->Path.Length) {
        fprintf(File, "x%p [label=\"%.*s\", shape=ellipse", (void *)Target, StrArg(Target->Path));
    }
    else {
        assert(0);
    }
    if (Target->NeedsRebuild) {
        fprintf(File, ", style=filled, fillcolor=orange");
    }
    fprintf(File, "]\n");


    for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
        target *Output = Target->Output.Elements[TargetIndex];
        fprintf(File, "x%p -> x%p\n", (void *)Target, (void *)Output);
        // Print this (sometimes again) because the main programs outputs wont get labels otherwise
        fprintf(File, "x%p [label=\"%.*s\", shape=ellipse", (void *)Output, StrArg(Output->Path));
        if (Output->NeedsRebuild) {
            fprintf(File, ", style=filled, fillcolor=orange");
        }
        fprintf(File, "]\n");
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

string ReplaceVariable(string VariableString, string VariableName, target_list *TargetList) {
    string Replacement = {};

    u64 BracketIndex = VariableName.Length;
    if (StringGetChar(VariableString, BracketIndex) == '[') {
        if (VariableString.Str[VariableString.Length-1] == ']') {
            string ArrayIndexStr = SubstrRange(VariableString, BracketIndex + 1, VariableString.Length-1);
            parsed_num ParsedIndex = StringParseNum(ArrayIndexStr);
            if (ParsedIndex.Error) {
                PushError("Error: invalid subscript for variable '%.*s'\n", StrArg(VariableString));
            }
            else if (ParsedIndex.Value >= TargetList->Count) {
                PushError("Error: subscript out of bounds (count: %lu) for variable '%.*s'\n", TargetList->Count, StrArg(VariableString));
            }
            else {
                Replacement = TargetString(TargetList->Elements[ParsedIndex.Value]);
            }
        }
        else {
            PushError("Error: invalid subscript for variable '%.*s'\n", StrArg(VariableString));
        }
    }
    else {
        if (TargetList->Count > 0) {
            Replacement = TargetListString(TargetList);
        }
        else {
            string TargetListType = {};
            if (StringMatch(VariableName, Strlit("OUTPUT"))) {
                TargetListType = Strlit("Output");
            }
            else if (StringMatch(VariableName, Strlit("INPUT"))) {
                TargetListType = Strlit("Input");
            }
            else {
                assert(0);
            }
            // TODO: Better error
            PushError("Error: No %.*s defined for target\n", StrArg(TargetListType));
        }
    }

    return Replacement;
}

struct replace_variables_result {
    bool Error;
    string String;
};

replace_variables_result ReplaceVariables(string String, target_list *Input, target_list *Output) {
    replace_variables_result Result = {};

    s_list StringList = {};

    for (u64 i = 0; i < String.Length;) {
        u64 OpenBraceIndex = StringFind(String, '{', i);

        string Part = SubstrRange(String, i, OpenBraceIndex);
        StringListAppend(&StringList, Part);

        if (OpenBraceIndex < String.Length) {
            u64 CloseBraceIndex = StringFind(String, '}', OpenBraceIndex + 1);
            if (CloseBraceIndex < String.Length) {
                string VariableString = SubstrRange(String, OpenBraceIndex + 1, CloseBraceIndex);
                string InputVar = Strlit("INPUT");
                string OutputVar = Strlit("OUTPUT");
                string Replacement = {};
                if (StringStartsWith(VariableString, InputVar)) {
                    Replacement = ReplaceVariable(VariableString, InputVar, Input);
                }
                else if (StringStartsWith(VariableString, OutputVar)) {
                    Replacement = ReplaceVariable(VariableString, OutputVar, Output);
                }
                else {
                    PushError("Error: Undefined variable '%.*s'\n", StrArg(VariableString));
                }

                if (Replacement.Length) {
                    StringListAppend(&StringList, Replacement);
                }
                else {
                    Result.Error = false;
                    break;
                }
            }
            else {
                StringListAppend(&StringList, SubstrRange(String, OpenBraceIndex, String.Length));
            }

            i = CloseBraceIndex + 1;
        }
        else {
            break;
        }
    }

    if (!Result.Error) {
        Result.String =  StringListJoin(&StringList);

    }

    return Result;
}

s32 RunBuildCommands()
{
    s32 ReturnCode = 0;

    if (State.CommandsToRunCount == 0) {
        printf("Nothing to rebuild.\n");
    }

    for (u64 TargetIndex = 0; TargetIndex < State.CommandsToRunCount; TargetIndex += 1) {
        target *Target = State.CommandsToRun[TargetIndex];

        // TODO: Maybe do this in BuildTarget?
        string TemplateArgs = Target->Args;
        replace_variables_result ReplaceVars = ReplaceVariables(TemplateArgs, &Target->Input, &Target->Output);
        if (ReplaceVars.Error) {
            ReturnCode = 1;
            break;
        }

        string Args = ReplaceVars.String;

        string Program = Target->Program;

        // Probably need to support user passing invdividual args, if not make that the only way
        // otherwise we need escaping, or ways for people to put things like apostropes in their args.
        string_list ArgList = {};
        arg_parser Parser = { .String = Args };

        while (!Parser.Error) {
            string Arg = GetArg(&Parser);
            if (Arg.Length == 0) {
                break;
            }

            ArrayAdd(&ArgList, Arg);
        }

        if (Parser.Error) {
            PushError("Error parsing command arguments '%.*s'\n", StrArg(Args));
            ReturnCode = 1;
            break;
        }

        printf("[%lu/%lu] %.*s %.*s\n", TargetIndex+1, State.CommandsToRunCount, StrArg(Program), StrArg(Args));

        assert(Target->Output.Count);
        assert(Target->Program.Length);

        bool Error = false;

        ReturnCode = OS_RunProcess(Program, ArgList.Data, ArgList.Count);
        if (ReturnCode == 0) {
            for (u64 OutputIndex = 0; OutputIndex < Target->Output.Count; OutputIndex += 1) {
                target *Output = Target->Output.Elements[OutputIndex];
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

        if (Error) {
            break;
        }
    }

    return ReturnCode;
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

target_list ParseDependencyFile(string String) {

    target_list List = {};

    // TODO: should we ensure the target of these matches what we expect

    // You can always search for ': ' to know end of target, because spaces will be escaped
    // Spaces can be used to separate dependencies also
    cut_result Parts = StringCut(String, Strlit(": "));
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
    // * Line/file numbers for errors, maybe uses C++ source location or macros


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


    string DependencyFile = Strlit("sample/build/main.d");
    read_entire_file_result ReadResult = OS_ReadEntireFile(DependencyFile);
    if (ReadResult.Error) {
        PushError("Error: Failed to read %.*s.", StrArg(DependencyFile));
        return 1;
    }

    target_list Headers = ParseDependencyFile(ReadResult.String);

    target *Main = Target({
        .Input = "sample/main.cpp",
        .Output = List("sample/build/main.o", "sample/build/main.d"),
        .Args = "-c {INPUT} -o {OUTPUT[0]} -g -MMD -MF {OUTPUT[1]}",
        .Program = "clang++",
        .Depends = Headers,
    });

    target *File1 = Target({
        .Input = "sample/file1.cpp",
        .Output = "sample/build/file1.o",
        .Args = "-c {INPUT} -o {OUTPUT} -g -MMD",
        .Program = "clang++",
    });

    target *Exe = Target({
        .Input = List(Main->Output.Elements[0], File1),
        .Output = "sample/build/main.exe",
        .Args = "-o {OUTPUT} {INPUT}",
        .Program = "clang++",
    });

    target *Mkdir = Target({
        .Output = "sample/stage",
        .Args = "-p {OUTPUT}",
        .Program = "mkdir",
    });

    target *Cp = Target({
        .Input = List(Exe, Mkdir),
        .Output = "sample/build/main.exe",
        .Args = "",
        .Program = "cp {OUTPUT[0]} {OUTPUT[1]}",
    });



    s32 ReturnCode = 0;

    build_result BuildResult = BuildTarget(Exe);
    if (BuildResult.Error) {
        ReturnCode = 1;
    }
    else {
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
            ReturnCode = RunBuildCommands();
        }
    }

    for (u64 ErrorIndex = 0; ErrorIndex < State.ErrorCount; ErrorIndex += 1) {
        printf("%.*s\n", StrArg(State.Errors[ErrorIndex]));
        ReturnCode = 1;
    }

    return ReturnCode;
}

