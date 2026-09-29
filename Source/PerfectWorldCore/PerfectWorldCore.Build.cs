using UnrealBuildTool;

public class PerfectWorldCore : ModuleRules
{
    public PerfectWorldCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        CppStandard = CppStandardVersion.Cpp20;
        // The portable core is also built file by file by CMake for CTest.
        // Unity blobs would merge files' private helpers into one translation
        // unit and let the two builds disagree, so compile each file alone.
        bUseUnity = false;
    }
}
