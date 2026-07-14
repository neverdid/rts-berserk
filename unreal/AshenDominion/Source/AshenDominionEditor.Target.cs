using UnrealBuildTool;
using System.Collections.Generic;

public class AshenDominionEditorTarget : TargetRules
{
    public AshenDominionEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.AddRange(new[] { "AshenCore", "AshenDominion" });
    }
}
