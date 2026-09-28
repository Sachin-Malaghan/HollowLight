using UnrealBuildTool;

public class Hollowlight : ModuleRules
{
	public Hollowlight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"SlateCore",
			"Slate",
			"RenderCore",
			"AudioMixer",
			"ApplicationCore"
		});

		// Private/Core is engine-agnostic C++ (also built by Tools/SimHarness).
		PrivateIncludePaths.Add(System.IO.Path.Combine(ModuleDirectory, "Private"));
	}
}
