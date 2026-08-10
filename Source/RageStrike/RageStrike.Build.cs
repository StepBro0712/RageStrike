using UnrealBuildTool;

public class RageStrike : ModuleRules
{
	public RageStrike(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"AssetRegistry",
			"Landscape",
			// сетевая игра через интернет: публичный адрес и проброс порта
			"HTTP",
			"Sockets",
			"Networking",
			"ApplicationCore",
			"RenderCore",
			"RHI"
		});
	}
}
