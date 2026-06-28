// Copyright Daniel Raquel. All Rights Reserved.

#include "WorldClaimComponent.h"
#include "WorldClaimRegistry.h"
#include "WorldClaims.h"
#include "GameFramework/Actor.h"

UWorldClaimComponent::UWorldClaimComponent()
{
	// Claims are static metadata; no per-frame work needed.
	PrimaryComponentTick.bCanEverTick = false;
}

FBox UWorldClaimComponent::GetClaimBounds() const
{
	// Local box centered on the component, transformed to world space. TransformBy
	// returns the AABB enclosing the (possibly rotated) box — what spatial queries want.
	const FBox LocalBox(-ClaimExtent, ClaimExtent);
	return LocalBox.TransformBy(GetComponentTransform());
}

bool UWorldClaimComponent::ContainsPoint(const FVector& WorldPoint) const
{
	// Precise oriented-box test: bring the point into component-local space and compare
	// against the half-extents (tighter than the world AABB under rotation).
	const FVector Local = GetComponentTransform().InverseTransformPosition(WorldPoint);
	return FMath::Abs(Local.X) <= ClaimExtent.X
		&& FMath::Abs(Local.Y) <= ClaimExtent.Y
		&& FMath::Abs(Local.Z) <= ClaimExtent.Z;
}

bool UWorldClaimComponent::HasAllTags(const FGameplayTagContainer& Tags) const
{
	return Tags.IsEmpty() || ClaimTags.HasAll(Tags);
}

bool UWorldClaimComponent::HasAnyTags(const FGameplayTagContainer& Tags) const
{
	return Tags.IsEmpty() || ClaimTags.HasAny(Tags);
}

void UWorldClaimComponent::BeginPlay()
{
	Super::BeginPlay();

	// Mirror discovery + policy tags onto the owner so tag-based consumers (PCG) can find
	// and filter this claim without any dependency on WorldClaims.
	if (AActor* OwnerActor = GetOwner())
	{
		// Umbrella discovery tag.
		if (!PCGActorTag.IsNone())
		{
			OwnerActor->Tags.AddUnique(PCGActorTag);
		}
		// Each claim gameplay tag as an actor FName tag, e.g. "Claim.Decoration.Suppress",
		// "Claim.POI.City" — so a PCG "Get Actor Data" filtered by that tag matches this actor.
		for (const FGameplayTag& Tag : ClaimTags)
		{
			OwnerActor->Tags.AddUnique(Tag.GetTagName());
		}
	}

	RegisterWithRegistry();
}

void UWorldClaimComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromRegistry();
	Super::EndPlay(EndPlayReason);
}

void UWorldClaimComponent::RegisterWithRegistry()
{
	UWorldClaimRegistry* Registry = UWorldClaimRegistry::Get(this);
	if (!Registry)
	{
		UE_LOG(LogWorldClaims, Warning,
			TEXT("UWorldClaimComponent on '%s': no UWorldClaimRegistry (not a game world?); claim not registered."),
			*GetNameSafe(GetOwner()));
		return;
	}

	CachedRegistry = Registry;
	Registry->RegisterClaim(this);
}

void UWorldClaimComponent::UnregisterFromRegistry()
{
	if (UWorldClaimRegistry* Registry = CachedRegistry.Get())
	{
		Registry->UnregisterClaim(this);
	}
	CachedRegistry.Reset();
}
