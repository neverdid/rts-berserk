using UnrealBuildTool;

public class AshenCore : ModuleRules
{
    public AshenCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        bEnableExceptions = false;
        bUseRTTI = false;

        PublicDependencyModuleNames.AddRange(new[] { "Core" });
    }
}
