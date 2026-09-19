using UnrealBuildTool;
using System.Collections.Generic;

public class PerfectWorldTarget : TargetRules
{
    public PerfectWorldTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new[] { "PerfectWorldCore", "PerfectWorld" });
    }
}
