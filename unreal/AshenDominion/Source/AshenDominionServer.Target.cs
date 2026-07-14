using UnrealBuildTool;
using System.Collections.Generic;

public class AshenDominionServerTarget : TargetRules
{
    public AshenDominionServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "AshenCore", "AshenDominion" });
    }
}
