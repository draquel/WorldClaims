// Copyright Daniel Raquel. All Rights Reserved.

using UnrealBuildTool;

public class WorldClaims : ModuleRules
{
	public WorldClaims(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				// Claims are identified and filtered by gameplay tags. This is the only
				// non-engine-core dependency — keeps the plugin consumable by PCG (which
				// reads claim tags), Map/Compass/AI without any cross-plugin code coupling.
				"GameplayTags",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
	}
}
