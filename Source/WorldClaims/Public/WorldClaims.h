// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWorldClaims, Log, All);

/**
 * WorldClaims module — a spatial registry of tagged, prioritized world claims.
 *
 * A "claim" is a semantic spatial region (a POI footprint, a player construction)
 * that carries meaning/policy/display but does NOT modify voxels. Terrain shape
 * changes go through VoxelWorlds' Edit Layer (runtime) or its generation
 * conditioning hook (gen-time); a placement typically produces both a terrain
 * change and a claim.
 *
 * The plugin is deliberately generic and PCG-/voxel-agnostic: it depends only on
 * Core/Engine/GameplayTags. Consumers integrate without code coupling —
 *  - PCG decoration reads claim actors by gameplay tag,
 *  - Map / Compass / AI call UWorldClaimRegistry's spatial query API,
 *  - VoxelWorlds generation conditioning is supplied game-side via a VoxelWorlds
 *    interface (not a dependency of this plugin).
 *
 * See Documentation/CLAIMS_ARCHITECTURE.md for the full design.
 */
class FWorldClaimsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FWorldClaimsModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FWorldClaimsModule>("WorldClaims");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("WorldClaims");
	}
};
