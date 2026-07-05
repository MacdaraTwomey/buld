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

list<string> MatchFiles(string Pattern) {
    list<string> Files = {};

    Pattern = ProjectPath(Pattern);

    os_wildcard_file_iter Iter = OS_WildcardFileIter(Pattern);
    for (string File = OS_WildcardFileIterNext(&Iter);
         !Iter.Done;
         File = OS_WildcardFileIterNext(&Iter)) {
        ListAdd(&Files, File);
    }

    if (Iter.Error) {
        fprintf(stderr, "Error globbing pattern '%.*s' %s\n", StrArg(Pattern), strerror(errno));
    }

    OS_WildcardFileIterClose(&Iter);

    return Files;
}

// TODO: Make recursive dirs
target *Mkdir(string Path) {
    return Target({
        .Output = Path,
        .CommandProc = [](target *Target) -> s32 {
            return OS_CreateDirectory(Target->Output.Data[0]->Name, OS_FileMode_DefaultDir, OS_CreateDirectoryFlags_AllowExists) ? 0 : 1;
        },
    });
}

struct executable_args {
    string Path;
    list<target *> Sources;
    list<string> CompileFlags;
    list<string> LinkFlags;
};

target *CppExecutable(executable_args ExecutableArgs) {

    string Dir = DirectoryFromPath(ExecutableArgs.Path);

    target *BuildDir = Mkdir(Dir);

    list<target *> ObjectFiles = {};

    for (u64 SourceIndex = 0; SourceIndex < ExecutableArgs.Sources.Count; SourceIndex += 1) {
        target *Source = ExecutableArgs.Sources.Data[SourceIndex];
        string FileNamePart = RemoveExtension(FilenameFromPath(Source->Path));

        string DependencyFile = StringConcat(Dir, FileNamePart, Str(".d"));
        string ObjectFilePath = StringConcat(Dir, FileNamePart, Str(".o"));

        os_read_entire_file_result ReadResult = OS_ReadEntireFile(DependencyFile);
        if (ReadResult.Error) {
            PushError("Error: Failed to read %.*s.", StrArg(DependencyFile));
        }

        list<target *> Headers = ParseDependencyFile(ReadResult.String);

        target *Object = Target({
            .Input = Source,
            .Output = {ObjectFilePath, DependencyFile},
            .Args = {
                ExecutableArgs.CompileFlags,
                "-c", "{INPUT}",
                "-o", "{OUTPUT[0]}",
                "-MMD", "-MF", "{OUTPUT[1]}"
            },
            .Program = Str("clang++"),
            .Depends = {Headers, BuildDir},
        });

        ListAdd(&ObjectFiles, Object->Output.Data[0]);
    }

    target *Exe = Target({
        .Input = ObjectFiles,
        .Output = ExecutableArgs.Path,
        .Args = {
            ExecutableArgs.LinkFlags,
            "-o", "{OUTPUT}",
            "{INPUT}"
        },
        .Program = Str("clang++"),
        .Depends = BuildDir,
    });

    return Exe;
}

