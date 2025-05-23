


#include "buld.h"


// Theory:
// You might as well just look at changed files first, because at minimum you will
// need to compare all objects to the program. Although looking at all objects timestamps
// and all source files timestamps is more work than just looking at the program and objects. 

int main(int ArgCount, char **Args) {

    string CppSources = Source(
        "sample/main1.cpp", 
        "sample/file1.cpp"
    );

    string CppObjects = Deps(CppSources, "build", ".o");


    string LinkCommand = "clang++ -o {} -fsantize-address {}";



    string CompileCommand = "clang++ -o {} -Md -O0 -c {}";

    string NeedsCompile = GetChanged(CppSources, CppObjects);
    
    IncrementalRunCommand(NeedsCompile, CompileCommand, CppObjects, CppSources); 
}

// main1: main.o file1.o library.dll 
// library: obj1.o obj2.o obj3.o


int main(int ArgCount, char **Args) {

    target_list CppSources = TargetList((target_list){0}
        "sample/main1.cpp", 
        "sample/file1.cpp"
    );

    string_array ObjectFilesList = FindReplace(CppSources, "sample/%.cpp", "build/%.o");

    string LinkCommand = "clang++ -o {Out} -fsantize-address {In}";
    string CompileCommand = "clang++ -o {Out} -Md -O0 -c {In}";

    //target_list Objects = TargetList{ .In = CppSources, Out = ObjectFilesList, Command = CompileCommand };
    target_list Objects = TargetList(CppSources, ObjectFilesList, CompileCommand);

    //target_list Program = TargetList{ .In = Objects, Out = StrArr("build/main1"), Command = LinkCommand };
    target_list Program = TargetList(Objects, StrArr("build/main1"), LinkCommand);
                           
    BuildTarget(Program.Data[0]);
}

