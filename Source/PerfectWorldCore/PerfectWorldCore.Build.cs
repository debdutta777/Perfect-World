using UnrealBuildTool;

public class PerfectWorldCore : ModuleRules
{
    public PerfectWorldCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.Add("Core");
        CppStandard = CppStandardVersion.Cpp20;
    }
}
