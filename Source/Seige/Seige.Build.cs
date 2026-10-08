using UnrealBuildTool;
public class Seige : ModuleRules
{
    public Seige(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // Many translation units share anonymous-namespace helper names (Deploy,
        // Flags, Rules, palette colours). Earlier builds only succeeded because
        // 'git status' listed every file as modified and the adaptive build
        // excluded them all from unity blobs. Make that explicit and deterministic.
        bUseUnity = false;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "InputCore", "Json", "JsonUtilities", "ProceduralMeshComponent"});
        PrivateDependencyModuleNames.AddRange(new string[] {"SlateCore", "RenderCore", "RHI"});
        RuntimeDependencies.Add("$(TargetOutputDir)/Rules/...", System.IO.Path.Combine(ModuleDirectory, "../../Rules/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/Interface/...", System.IO.Path.Combine(ModuleDirectory, "../../Interface/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/AIFILES/...", System.IO.Path.Combine(ModuleDirectory, "../../AIFILES/..."), StagedFileType.NonUFS);
        RuntimeDependencies.Add("$(TargetOutputDir)/Graphics/...", System.IO.Path.Combine(ModuleDirectory, "../../Graphics/..."), StagedFileType.NonUFS);
    }
}
