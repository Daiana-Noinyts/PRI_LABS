
using UnrealBuildTool;

public class Forest : ModuleRules
{
	public Forest(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { 
			"Core", 
			"CoreUObject", 
			"Engine", 
			"InputCore", 
			"EnhancedInput", 
			"UMG", 
			"Slate", 
			"SlateCore", 
			"PhysicsCore",
			"AIModule",
			"GameplayTasks",
			"NavigationSystem",
			"Niagara"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
