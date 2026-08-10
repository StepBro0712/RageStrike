using UnrealBuildTool;
using System.Collections.Generic;

public class RageStrikeTarget : TargetRules
{
	public RageStrikeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RageStrike");
	}
}
