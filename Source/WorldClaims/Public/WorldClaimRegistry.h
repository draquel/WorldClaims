// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "GameplayTagContainer.h"
#include "WorldClaimRegistry.generated.h"

class UWorldClaimComponent;

/**
 * Fired when a claim is registered or unregistered, carrying the affected world-space bounds.
 * Consumers (e.g. the PCG re-decoration bridge) use it to dirty the region the claim covers.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnWorldClaimsChanged, const FBox& /*ChangedBounds*/);

/**
 * Authoritative per-world registry of active claims.
 *
 * A world subsystem (game worlds only). UWorldClaimComponents register on BeginPlay
 * and unregister on EndPlay. The registry answers spatial + tag queries for the
 * "depends on WorldClaims" consumers — Map, Compass, AI:
 *  - what claims overlap this region?
 *  - which highest-priority claim governs this point?
 *
 * PCG decoration does NOT use this API — it discovers claims by the mirrored actor tag
 * (UWorldClaimComponent::PCGActorTag), keeping VoxelWorlds/PCG free of any WorldClaims
 * code dependency.
 *
 * MVP storage is a flat array scanned per query (claims are few and static-ish). A
 * spatial acceleration structure is a later optimization if claim counts grow.
 */
UCLASS()
class WORLDCLAIMS_API UWorldClaimRegistry : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Convenience accessor: the registry for WorldContext's world, or null.
	 * The canonical way for consumers (Map / Compass / AI) to reach the registry.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Claims",
		meta = (WorldContext = "WorldContext", DisplayName = "Get World Claim Registry"))
	static UWorldClaimRegistry* Get(const UObject* WorldContext);

	/** Register a claim. Idempotent (no duplicate entries). */
	void RegisterClaim(UWorldClaimComponent* Claim);

	/** Remove a claim (e.g. its owner was destroyed). Safe if not registered. */
	void UnregisterClaim(UWorldClaimComponent* Claim);

	/**
	 * Claims whose footprint AABB overlaps QueryBounds and that pass the tag filter.
	 * @param QueryBounds   World-space box to test against.
	 * @param RequiredTags  Tag filter (empty = match all).
	 * @param bRequireAllTags  true = claim must have ALL RequiredTags; false = ANY.
	 */
	void QueryClaims(const FBox& QueryBounds, const FGameplayTagContainer& RequiredTags,
		bool bRequireAllTags, TArray<UWorldClaimComponent*>& OutClaims) const;

	/**
	 * Blueprint-friendly box query (center + half-extent rather than FBox).
	 */
	UFUNCTION(BlueprintCallable, Category = "World Claims", meta = (DisplayName = "Query Claims In Box"))
	void QueryClaimsInBox(const FVector& Center, const FVector& HalfExtent,
		const FGameplayTagContainer& RequiredTags, bool bRequireAllTags,
		TArray<UWorldClaimComponent*>& OutClaims) const;

	/**
	 * The highest-priority claim that contains WorldPoint and passes the tag filter,
	 * or null if none. Ties break toward the first registered.
	 */
	UFUNCTION(BlueprintCallable, Category = "World Claims")
	UWorldClaimComponent* GetHighestPriorityClaimAt(const FVector& WorldPoint,
		const FGameplayTagContainer& RequiredTags, bool bRequireAllTags) const;

	/** All currently registered claims (compacts dead entries as a side effect). */
	UFUNCTION(BlueprintCallable, Category = "World Claims")
	void GetAllClaims(TArray<UWorldClaimComponent*>& OutClaims) const;

	/** Number of live registered claims. */
	UFUNCTION(BlueprintCallable, Category = "World Claims")
	int32 GetClaimCount() const;

	/**
	 * Fired on register/unregister with the changed claim's bounds. Used by the PCG re-decoration
	 * bridge (Phase 7) to dirty + regenerate decoration overlapping the changed region.
	 */
	FOnWorldClaimsChanged OnClaimsChanged;

protected:
	// Game worlds only (PIE + standalone), never editor preview worlds.
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

private:
	/** Tag filter helper shared by all queries. */
	static bool PassesTagFilter(const UWorldClaimComponent& Claim,
		const FGameplayTagContainer& RequiredTags, bool bRequireAllTags);

	/**
	 * Weak refs — the owning actors hold the components; the registry never keeps a
	 * claim alive. Not a UPROPERTY (TArray<TWeakObjectPtr> needs no GC reachability);
	 * mutable so const queries can lazily drop stale entries.
	 */
	mutable TArray<TWeakObjectPtr<UWorldClaimComponent>> RegisteredClaims;
};
