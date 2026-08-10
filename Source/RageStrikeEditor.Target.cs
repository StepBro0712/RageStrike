using UnrealBuildTool;
using System.Collections.Generic;

public class RageStrikeEditorTarget : TargetRules
{
	public RageStrikeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RageStrike");
	}
}
