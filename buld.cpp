#include "src/buld.h"
#include "src/extra.h"

enum build_mode {
    BuildMode_Debug,
    BuildMode_Release,
};

s32 RebuildSelf(char **Argv, string SourcePath) {
    s32 ReturnCode = 0;

    string Filename = FilenameFromPath(SourcePath);
    string FilenameNoExt = RemoveExtension(Filename);
    string Dir = DirectoryFromPath(SourcePath);

    string ProgramPath = OS_GetCurrentExecutablePath();

    string DependencyFile = StringConcat(Dir, FilenameNoExt, Strlit(".d"));
    read_entire_file_result ReadResult = OS_ReadEntireFile(DependencyFile);
    if (ReadResult.Error) {
        PushError("Warning: Failed to read %.*s.", StrArg(DependencyFile));
    }

    list<target *> Headers = ParseDependencyFile(ReadResult.String);

    // TODO: Run compiler to get list of headers rather than read .d file?
    // This does force you to run compiler every time.

    target *BuldProgram = Target({
        .Input = SourcePath,
        .Output = { ProgramPath, DependencyFile },
        .Args = {
            "{INPUT}",
            "-o", "{OUTPUT[0]}",
            "-g",
            "-std=c++20",
            "-Wall", "-pedantic-errors", "-Wno-unused-variable", "-Wno-gnu-anonymous-struct", "-Wno-writable-strings",
            "-Wno-nested-anon-types", "-Wno-gnu-zero-variadic-macro-arguments", "-Wno-missing-braces",
            "-fno-strict-aliasing", "-Wno-unused-function", "-Wno-language-extension-token", "-Wno-deprecated-declarations",
            "-MMD", "-MF", "{OUTPUT[1]}"},
        .Program = Strlit("clang++"),
        .Depends = Headers,
    });

    build_result BuildResult = BuildTarget(BuldProgram);
    if (BuildResult.Error) {
        ReturnCode = 1;
        return ReturnCode;
    }
    else if (BuildResult.NeedsRebuild) {
        fprintf(stdout, "Rebuilding %.*s\n\n", StrArg(SourcePath));

        ReturnCode = RunBuildCommands(false);

        if (ReturnCode == 0) {
            fprintf(stdout, "\n");
            bool Ok = OS_ReplaceCurrentProcess(ProgramPath, Argv);
            if (!Ok) {
                ReturnCode = 1;
            }
        }
    }

    State = {};

    return ReturnCode;
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

    s32 ReturnCode = RebuildSelf(Args, Strlit(__FILE__));
    if (ReturnCode != 0) {
        return ReturnCode;
    }

    list<string> AA = MatchFiles(Strlit("/home/mac/projects/buld/**/*.h"));

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
    // * Allow passing targets into args to opt out of using variables (adds them as inputs maybe)

    // If might be a nicer data structure to have files as inputs and outputs of other files, and some files
    // just have a pointer to the command to run to generate them (maybe the command points to the inputs and outputs also? probably not necessary)
    // shouldn't affect user api though


    State.ProjectRoot = Strlit("/home/mac/projects/buld/");
    //State.ProjectRoot = DirectoryFromPath(Strlit(__FILE__));
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
        .Input = "sample/main.cpp",
        .Output = {"sample/build/main.o", "sample/build/main.d"},
        .Args = {"-c", "{INPUT}", "-o", "{OUTPUT[0]}", "-g", "-MMD", "-MF", "{OUTPUT[1]}"},
        .Program = Strlit("clang++"),
        .Depends = Headers,
    });

    target *File1 = Target({
        .Input = "sample/file1.cpp",
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

