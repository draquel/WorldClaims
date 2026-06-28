// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "NativeGameplayTags.h"

/**
 * Canonical native gameplay tags for the WorldClaims system.
 *
 * Registered natively at module load (no DataTable/ini needed) so they appear in the
 * tag picker and exist on every machine. Two axes:
 *  - **Type** (`Claim.POI.*`, `Claim.Construction.*`) — what produced the claim.
 *  - **Decoration policy** (`Claim.Decoration.*`) — how PCG should treat the footprint.
 *
 * `UWorldClaimComponent` mirrors a claim's gameplay tags onto its owning actor as FName
 * tags at BeginPlay, so PCG decoration graphs can discover and filter claims via
 * "Get Actor Data" by tag — with no dependency on this plugin (boundary-safe).
 */
namespace WorldClaimTags
{
	// Umbrella / discovery tag (also mirrored via UWorldClaimComponent::PCGActorTag).
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim);

	// Claim type — set by the producer (POI placement, player construction).
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_POI);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_POI_City);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_POI_Dungeon);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Construction);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Construction_Camp);

	// Decoration policy — how PCG should treat ambient decoration inside the footprint.
	// Suppress = bare footprint; OwnGraph = POI supplies its own decoration graph;
	// InheritBiome = use ambient per-biome decoration; Blend = falloff transition ring.
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Decoration_Suppress);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Decoration_OwnGraph);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Decoration_InheritBiome);
	WORLDCLAIMS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Claim_Decoration_Blend);
}
