using UnrealBuildTool;
public class Seige : ModuleRules
{
    public Seige(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "ProceduralMeshComponent"});
        RuntimeDependencies.Add("$(TargetOutputDir)/Rules/...", System.IO.Path.Combine(ModuleDirectory, "../../Rules/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/Interface/...", System.IO.Path.Combine(ModuleDirectory, "../../Interface/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/AIFILES/...", System.IO.Path.Combine(ModuleDirectory, "../../AIFILES/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/Graphics/...", System.IO.Path.Combine(ModuleDirectory, "../../Graphics/..."), StagedFileType.NonUFS);
    }
}
