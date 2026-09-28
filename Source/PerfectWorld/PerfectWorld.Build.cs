using UnrealBuildTool;

public class PerfectWorld : ModuleRules
{
    public PerfectWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "PerfectWorldCore"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore" });
    }
}
