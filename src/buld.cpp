
#include "buld.h"

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

target NullTarget = {};

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

string ProjectPath(string Path) {
    return StringConcat(State.ProjectRoot, Path);
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

void WriteGraphNode(FILE *File, target *Target) {
    fprintf(File, "x%p [label=\"", (void *)Target);
    if (Target->Program.Length) {
        string Args = ArgsToString(&Target->Args);
        fprintf(File, "%.*s %.*s", StrArg(Target->Program), StrArg(Args));
    }
    else if (Target->Path.Length) {
        fprintf(File, "%.*s", StrArg(Target->Path));
    }
    else {
        assert(0);
    }

    time_t ModTime = (time_t)Target->FileInfo.ModTime;
    struct tm *TimeInfo = localtime(&ModTime);
    char Buffer[100] = {};
    strftime(Buffer, sizeof(Buffer), "%H:%M:%S", TimeInfo);
    fprintf(File, "\\nexist: %s, mod: %s, visit: %lu\"",
            Target->FileInfo.Exists ? "yes" : "no",
            ModTime == 0 ? "-" : Buffer,
            Target->VisitCount);

    if (Target->Program.Length) {
        fprintf(File, ", shape=box");
    }
    else if (Target->Path.Length) {
        fprintf(File, ", shape=ellipse");
    }

    if (Target->Path.Length && Target->VisitCount > 0 && !Target->FileInfo.Exists) {
        fprintf(File, ", style=filled, fillcolor=coral2");
    }
    else if (Target->NeedsRebuild) {
        fprintf(File, ", style=filled, fillcolor=orange");
    }

    fprintf(File, "]\n");
}

void WriteGraphTarget(FILE *File, target *Target) {
    if (Target->ParentCommand) {
        WriteGraphTarget(File, Target->ParentCommand);
    }
    else {
        for (u64 TargetIndex = 0; TargetIndex < Target->Input.Count; TargetIndex += 1) {
            target *Input = Target->Input.Data[TargetIndex];
            WriteGraphTarget(File, Input);
            fprintf(File, "x%p -> x%p\n", (void *)Input, (void *)Target);
        }
        for (u64 TargetIndex = 0; TargetIndex < Target->Depends.Count; TargetIndex += 1) {
            target *Depends = Target->Depends.Data[TargetIndex];
            WriteGraphTarget(File, Depends);
            fprintf(File, "x%p -> x%p [style=dotted]\n", (void *)Depends, (void *)Target);
        }
    }

    WriteGraphNode(File, Target);

    for (u64 TargetIndex = 0; TargetIndex < Target->Output.Count; TargetIndex += 1) {
        target *Output = Target->Output.Data[TargetIndex];
        fprintf(File, "x%p -> x%p\n", (void *)Target, (void *)Output);
    }
}

void BeginGraph(FILE *File) {
    // strict makes it so two identical edges are only drawn once
    fprintf(File, "strict digraph G {\n");
}

void EndGraph(FILE *File) {
    fprintf(File, "}\n");
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

struct arg_iter {
    string String;
    u64 At;
};

struct arg_iter_item {
    string String;
    bool IsVariable;
    bool Done;
    bool IsEntireArg;
};

// We force any opened and closed braces to contain variables, otherwise braces must be escaped
arg_iter_item ArgIterNext(arg_iter *Iter) {
    arg_iter_item Item = {};

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
            u64 CloseBraceIndex = StringFind(Iter->String, '}', Iter->At + 1);
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

    arg_iter Iter = { .String = Arg };

    s_list Builder = {};
    for (arg_iter_item Item = ArgIterNext(&Iter); !Item.Done; Item = ArgIterNext(&Iter)) {
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

enum build_mode {
    BuildMode_Debug,
    BuildMode_Release,
};

void RecompileBuld(string SourceFile, string ProgramName) {
    os_file_info
}

//list<target *>::list<target *>(const char *String) {
//    *this = {};
//    ListAdd(this, CreateString((char *)String));
//}
//list<target *>::list<target *>(string String) {
//    *this = {};
//    ListAdd(this, String);
//}
//list<target *>::list<target *>(target *Target) {
//    *this = {};
//    ListAdd(this, Target);
//}

int main(int ArgCount, char **Args) {

    RecompileBuld(Strlit(__FILE__), Strlit("buld"));

    string GraphPath = {};
    bool ForceRebuild = false;
    bool DryRun = false;
    build_mode BuildMode = BuildMode_Debug;

    list<string> BuildTargets = {};

    for (s32 ArgIndex = 1; ArgIndex < ArgCount; ArgIndex += 1) {
        string Arg = CreateString(Args[ArgIndex]);
        if (StringMatch(Arg, Strlit("--graph"))) {
            ArgIndex += 1;
            if (ArgIndex < ArgCount) {
                GraphPath = CreateString(Args[ArgIndex]);
            }
            else {
                fprintf(stderr, "Error: missing value for arg --graph.\n");
                return 1;
            }
        }
        else if (StringMatch(Arg, Strlit("--force"))) {
            ForceRebuild = true;
        }
        else if (StringMatch(Arg, Strlit("--release"))) {
            BuildMode = BuildMode_Release;
        }
        else if (StringMatch(Arg, Strlit("--debug"))) {
            BuildMode = BuildMode_Debug;
        }
        else if (StringMatch(Arg, Strlit("--dry-run"))) {
            DryRun = true;
        }
        else {
            if (StringMatch(StringPrefix(Arg, 2), Strlit("--"))) {
                fprintf(stderr, "Error: invalid optional argument '%.*s'.\n", StrArg(Arg));
                return 1;
            }
            else {
                ListAdd(&BuildTargets, Arg);
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
    // * Embedding
    // * Hash output files so they are only used in later commands if they actually changed?


    State.ProjectRoot = Strlit("/home/mac/projects/buld/");
    State.ForceRebuild = ForceRebuild;

    if (BuildMode == BuildMode_Debug) {
        //PushFlags("-g", Strlit("-O0"));
    }
    else if (BuildMode == BuildMode_Release) {
        //PushFlags("-O3");
    }


    //target *GenSrc = Target({
    //    .Input = "sample/gen-src.txt",
    //    .Output = List("sample/build/gen-src.cpp", "sample/build/gen-src.h"),
    //    .Args = "--do-gen",
    //    .Program = "gener",
    //});
    //target *GenObj = Target({
    //    .Input = GenSrc, // GenSrc->Output.Data[0],
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

    list<target *> Headers = ParseDependencyFile(ReadResult.String);

    target *Main = Target({
        .Input = Strlit("sample/main.cpp"),
        .Output = {"sample/build/main.o", "sample/build/main.d"},
        .Args = {"-c", "{INPUT}", "-o", "{OUTPUT[0]}", "-g", "-MMD", "-MF", "{OUTPUT[1]}"},
        .Program = Strlit("clang++"),
        .Depends = Headers,
    });

    target *File1 = Target({
        .Input = Strlit("sample/file1.cpp"),
        .Output = "sample/build/file1.o",
        .Args = {"-c", "{INPUT}", "-o", "{OUTPUT}", "-g", "-MMD"},
        .Program = Strlit("clang++"),
    });

    target *Exe = Target({
        .Input = { Main->Output.Data[0], File1 },
        .Output = "sample/build/main.exe",
        .Args = {"-o", "{OUTPUT}", "{INPUT}"},
        .Program = Strlit("clang++"),
    });

    target *StageDir = Target({
        .Output = "sample/stage",
        .Args = {"-p", "{OUTPUT}"},
        .Program = Strlit("mkdir"),
    });

    Target({
        .Name = Strlit("copy"),
        .Input = { Exe, StageDir },
        .Args = {"{INPUT[0]}", "{INPUT[1]}/."},
        .Program = Strlit("cp"),
    });

    if (BuildTargets.Count == 0) {
        ArrayAdd(&BuildTargets, Strlit("sample/build/main.exe"));
    }

    s32 ReturnCode = 0;

    for (u64 TargetIndex = 0; TargetIndex < BuildTargets.Count; TargetIndex += 1) {
        string TargetName = BuildTargets.Data[TargetIndex];
        target *Target = FindTarget(TargetName);
        if (Target == &NullTarget) {
            fprintf(stderr, "Error: unknown target '%.*s'.\n", StrArg(TargetName));
            return 1;
        }

        build_result BuildResult = BuildTarget(Target);
        if (BuildResult.Error) {
            ReturnCode = 1;
            break;
        }
    }

    if (GraphPath.Length) {
        // TODO: No fopen and real string handling
        FILE *GraphFile = fopen((char *)GraphPath.Str, "wb");
        if (!GraphFile) {
            fprintf(stderr, "Error: failed to open --graph file %.*s. %s\n", StrArg(GraphPath), strerror(errno));
            return 1;
        }

        BeginGraph(GraphFile);
        for (u64 TargetIndex = 0; TargetIndex < State.TargetCount; TargetIndex += 1) {
            target *Target = State.Targets + TargetIndex;
            WriteGraphTarget(GraphFile, Target);
        }
        EndGraph(GraphFile);

        fclose(GraphFile);
    }

    if (ReturnCode == 0) {
        ReturnCode = RunBuildCommands(DryRun);
    }

    for (u64 ErrorIndex = 0; ErrorIndex < State.ErrorCount; ErrorIndex += 1) {
        printf("%.*s\n", StrArg(State.Errors[ErrorIndex]));
        ReturnCode = 1;
    }

    return ReturnCode;
}

