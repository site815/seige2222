using UnrealBuildTool;
using System.Collections.Generic;
public class SeigeTarget : TargetRules
{
    public SeigeTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("Seige");
    }
}
