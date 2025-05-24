
#include "buld.h"

// main1: main.o file1.o library.dll 
// library: obj1.o obj2.o obj3.o


int main(int ArgCount, char **Args) {

    string_list CppFiles = {0};
    ArrayAdd(&CppFiles, Str("sample/main1.cpp"));
    ArrayAdd(&CppFiles, Str("sample/file1.cpp"));
    target_list CppSources = Sources(CppFiles);

    string_list ObjectFilesList = FindReplace(CppFiles, Str("sample/%.cpp"), Str("build/%.o"));

    string LinkCommand = Str("clang++ -o {Out} -fsantize-address {In}");
    string CompileCommand = Str("clang++ -o {Out} -Md -O0 -c {In}");

    do {
        target_list Objects = TargetList(ObjectFilesList, CppSources);
        for (target *It = TargetIterNext(0, &Objects); It; It = TargetIterNext(It, &Objects)) {
            if (OS_RunCommand(CompileCommand, It->Name, ToStringList(It->Prereqs)) != 0) {
                break;
            }
        }

        target Program = Target(Str("build/main1"), Objects);
        if (Program.NeedsRebuild) {
            if (OS_RunCommand(LinkCommand, Program.Name, ToStringList(Program.Prereqs)) != 0) {
                break;
            }
        }
    } while (0);

    // maybe a IncrementalBuild function is better, but allow people to drop into lower level api in 
    // a less simple way
    // Could write in c++ or go, c is not the best fit
    // Also need way of reading .d make files, so need way of adding additional non-target pre-requisites


}

