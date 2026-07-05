#include "src/buld.h"
#include "src/extra.h"

enum build_mode {
    BuildMode_Debug,
    BuildMode_Release,
};

int main(int ArgCount, char **Args) {

    RebuildSelf(Args, Str(__FILE__));

    string GraphPath = {};
    build_mode BuildMode = BuildMode_Debug;

    list<string> BuildTargetList = {};

    for (s32 ArgIndex = 1; ArgIndex < ArgCount; ArgIndex += 1) {
        string Arg = CreateString(Args[ArgIndex]);
        if (StringMatch(Arg, Str("--graph"))) {
            ArgIndex += 1;
            if (ArgIndex < ArgCount) {
                GraphPath = CreateString(Args[ArgIndex]);
            }
            else {
                fprintf(stderr, "Error: missing value for arg --graph.\n");
                return 1;
            }
        }
        else if (StringMatch(Arg, Str("--force-rebuild"))) {
            State.ForceRebuild = true;
        }
        else if (StringMatch(Arg, Str("--release"))) {
            BuildMode = BuildMode_Release;
        }
        else if (StringMatch(Arg, Str("--debug"))) {
            BuildMode = BuildMode_Debug;
        }
        else if (StringMatch(Arg, Str("--dry-run"))) {
            State.DryRun = true;
        }
        else {
            if (StringMatch(StringPrefix(Arg, 2), Str("--"))) {
                fprintf(stderr, "Error: invalid optional argument '%.*s'.\n", StrArg(Arg));
                return 1;
            }
            else {
                ListAdd(&BuildTargetList, Arg);
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

    State.ProjectRoot = DirectoryFromPath(Str(__FILE__));

    list<string> CompileFlags = {};
    if (BuildMode == BuildMode_Debug) {
        CompileFlags += "-g";
        CompileFlags += "-O0";
    }
    else if (BuildMode == BuildMode_Release) {
        CompileFlags += "-O3";
    }

    target *Exe = CppExecutable({
        .Path = Str("sample/build/main.exe"),
        .Sources = {
            MatchFiles(Str("sample/*.cpp"))
            //"sample/file1.cpp"
            //"sample/main.cpp",
        },
        .CompileFlags = CompileFlags,
        .LinkFlags = {},
    });

    Target({
        .Name = Str("copy"),
        .Input = {
            Exe,
            Mkdir(Str("sample/stage"))
        },
        .Args = {"{INPUT[0]}", "{INPUT[1]}/."},
        .Program = Str("cp"),
    });

    if (BuildTargetList.Count == 0) {
        ListAdd(&BuildTargetList, Exe->Output.Data[0]->Path);
    }

    s32 ReturnCode = BuildTargetByName(&BuildTargetList);

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
        ReturnCode = RunBuildCommands();
    }

    for (u64 ErrorIndex = 0; ErrorIndex < State.ErrorCount; ErrorIndex += 1) {
        fprintf(stderr, "%.*s\n", StrArg(State.Errors[ErrorIndex]));
        ReturnCode = 1;
    }

    return ReturnCode;
}

