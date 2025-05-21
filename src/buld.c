


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

    target CppSources = Source(
        "sample/main1.cpp", 
        "sample/file1.cpp"
    );

    string LinkCommand = "clang++ -o {Out} -fsantize-address {In}";
    string CompileCommand = "clang++ -o {Out} -Md -O0 -c {In}";

    target Objects = Target((target){ In = CppSources, Out = "build/*.o", Command = CompileCommand });

    target Program = Target((target){ In = Objects, Out = "build/main1", LinkCommand);
                           
    BuildTarget(Program);
}
