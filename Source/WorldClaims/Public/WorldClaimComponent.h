// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "WorldClaimTypes.h"
#include "WorldClaimComponent.generated.h"

class UWorldClaimRegistry;

/**
 * Makes its owning actor a world claim.
 *
 * Attach to any actor (POI marker, player construction, designer volume) to register
 * a tagged, prioritized spatial footprint with the UWorldClaimRegistry. The claim is
 * pure semantics — it never touches voxels. Terrain shaping is a separate path
 * (VoxelWorlds Edit Layer at runtime, or the generation conditioning hook at gen-time).
 *
 * On BeginPlay the component:
 *  1. mirrors PCGActorTag onto the owning actor, so PCG decoration graphs can find
 *     claims via "Get Actor Data" filtered by tag (no PCG dependency here), and
 *  2. registers itself with the world's UWorldClaimRegistry for Map/Compass/AI queries.
 *
 * Footprint shape for the registry MVP is an oriented box defined by ClaimExtent at the
 * component's world transform. (Spline/volume shapes are a later refinement — see
 * CLAIMS_ARCHITECTURE.md "Open questions".)
 */
UCLASS(ClassGroup = (WorldClaims), meta = (BlueprintSpawnableComponent), HideCategories = (Physics, Collision, Lighting, Rendering))
class WORLDCLAIMS_API UWorldClaimComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWorldClaimComponent();

	/** Semantic tags: Claim.POI.City, Claim.Construction.Camp, Claim.Decoration.Suppress, etc. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Claim")
	FGameplayTagContainer ClaimTags;

	/** Higher priority wins where footprints overlap (decoration policy, map layering). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Claim")
	int32 Priority = 0;

	/** Map / Compass presentation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Claim")
	FWorldClaimDisplayInfo DisplayInfo;

	/** Box half-extents (component local space) describing the claim footprint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Claim")
	FVector ClaimExtent = FVector(500.0f, 500.0f, 500.0f);

	/**
	 * Actor tag mirrored onto the owner at BeginPlay so PCG (and any tag-based consumer)
	 * can discover claims without depending on this plugin. Empty = don't tag the owner.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "World Claim")
	FName PCGActorTag = FName(TEXT("Claim"));

	/** World-space axis-aligned bounds of the (possibly rotated) claim box. */
	UFUNCTION(BlueprintCallable, Category = "World Claim")
	FBox GetClaimBounds() const;

	/** True if WorldPoint is inside the oriented claim box. */
	UFUNCTION(BlueprintCallable, Category = "World Claim")
	bool ContainsPoint(const FVector& WorldPoint) const;

	/** True if this claim carries every tag in Tags (empty Tags = always true). */
	UFUNCTION(BlueprintCallable, Category = "World Claim")
	bool HasAllTags(const FGameplayTagContainer& Tags) const;

	/** True if this claim carries any tag in Tags (empty Tags = always true). */
	UFUNCTION(BlueprintCallable, Category = "World Claim")
	bool HasAnyTags(const FGameplayTagContainer& Tags) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Resolve the registry and register this claim. Idempotent. */
	void RegisterWithRegistry();

	/** Unregister from the cached registry, if still valid. */
	void UnregisterFromRegistry();

	/** Cached so EndPlay can unregister even if world teardown order varies. */
	TWeakObjectPtr<UWorldClaimRegistry> CachedRegistry;
};
