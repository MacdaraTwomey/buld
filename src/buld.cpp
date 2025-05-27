
#include "buld.h"

// main1: main.o file1.o library.dll 
// library: obj1.o obj2.o obj3.o

// Other ideas and features
// way to have read, write, copy, delete file operations that can be done incrementally
// globals aren't that bad for a program like this

int main(int ArgCount, char **Args) {

    string_list CppFiles = {0};
    ArrayAdd(&CppFiles, Str("sample/main1.cpp"));
    ArrayAdd(&CppFiles, Str("sample/file1.cpp"));
    target_list CppSources = TargetList(CppFiles, {}, {});

    string_list ObjectFilesList = FindReplaceList(CppFiles, Str("sample/%.cpp"), Str("build/%.o"));

    // TODO: Mkdir missing build output directories (when?)
    string LinkCommand = Str("clang++ -o {Out} -fsantize-address {In}");
    string CompileCommand = Str("clang++ -o {Out} -O0 -c {In}");

    target_list Objects = TargetList(ObjectFilesList, CppSources, CompileCommand);
    target Program = Target(Str("build/main1"), Objects, LinkCommand);

    bool TargetBuilt = BuildTarget(&Program);

    // maybe a IncrementalBuild function is better, but allow people to drop into lower level api in 
    // a less simple way
    // Could write in c++ or go, c is not the best fit
    // Also need way of reading .d make files, so need way of adding additional non-target pre-requisites


}

