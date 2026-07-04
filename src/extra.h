#pragma once

#include "buld.h"

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

list<string> MatchFiles(string Pattern) {
    list<string> Files = {};

    os_wildcard_file_iter Iter = OS_WildcardFileIter(Pattern);
    for (string File = OS_WildcardFileIterNext(&Iter);
         !Iter.Done;
         File = OS_WildcardFileIterNext(&Iter)) {
        //printf("'%.*s'\n", StrArg(File));
    }

    if (Iter.Error) {
        fprintf(stderr, "Error globbing pattern '%.*s' %s\n", StrArg(Pattern), strerror(errno));
    }

    OS_WildcardFileIterClose(&Iter);

    return Files;
}