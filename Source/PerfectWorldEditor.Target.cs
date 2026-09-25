using UnrealBuildTool;
using System.Collections.Generic;

public class PerfectWorldEditorTarget : TargetRules
{
    public PerfectWorldEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.AddRange(new[] { "PerfectWorldCore", "PerfectWorld" });
    }
}
