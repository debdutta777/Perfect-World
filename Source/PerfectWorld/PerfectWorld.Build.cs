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
        // Slate draws the front end (title, creation flow, pause menu) in code, with no UI assets.
        // EnhancedInput drives the walking player; its actions and mappings are built in code, so no input assets exist.
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "Slate", "SlateCore", "EnhancedInput" });
    }
}
