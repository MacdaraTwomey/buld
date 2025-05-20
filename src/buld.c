


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

int main(int ArgCount, char **Args) {

    file_array CppSources = Source(
        "sample/main1.cpp", 
        "sample/file1.cpp"
    );

    string LinkCommand = "clang++ -o {Out} -fsantize-address {In}";
    string CompileCommand = "clang++ -o {Out} -Md -O0 -c {In}";

    target Target = TwoStageTarget((stage){ In = CppSources, Out = "build/%.o", Command = CompileCommand },
                                   (stage){ In = STAGE_PREV, Out = "build/main1", LinkCommand);
                           
    IncrementalRunCommand(Target);
}
