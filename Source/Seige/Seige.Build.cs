using UnrealBuildTool;
public class Seige : ModuleRules
{
    public Seige(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities"});
        RuntimeDependencies.Add("$(TargetOutputDir)/Rules/...", System.IO.Path.Combine(ModuleDirectory, "../../Rules/..."), StagedFileType.NonUFS);
    }
}
