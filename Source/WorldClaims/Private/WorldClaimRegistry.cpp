// Copyright Daniel Raquel. All Rights Reserved.

#include "WorldClaimRegistry.h"
#include "WorldClaimComponent.h"
#include "WorldClaims.h"
#include "Engine/World.h"

UWorldClaimRegistry* UWorldClaimRegistry::Get(const UObject* WorldContext)
{
	if (!WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWorldClaimRegistry>() : nullptr;
}

bool UWorldClaimRegistry::ShouldCreateSubsystem(UObject* Outer) const
{
	if (const UWorld* World = Cast<UWorld>(Outer))
	{
		return World->IsGameWorld();
	}
	return false;
}

void UWorldClaimRegistry::Deinitialize()
{
	RegisteredClaims.Reset();
	Super::Deinitialize();
}

void UWorldClaimRegistry::RegisterClaim(UWorldClaimComponent* Claim)
{
	if (!Claim)
	{
		return;
	}

	// Idempotent: a component should never register twice, but guard anyway.
	if (RegisteredClaims.Contains(Claim))
	{
		return;
	}

	RegisteredClaims.Add(Claim);
	UE_LOG(LogWorldClaims, Verbose, TEXT("Registered claim '%s' (priority %d, %d tag(s)); %d total."),
		*GetNameSafe(Claim->GetOwner()), Claim->Priority, Claim->ClaimTags.Num(), RegisteredClaims.Num());
}

void UWorldClaimRegistry::UnregisterClaim(UWorldClaimComponent* Claim)
{
	if (!Claim)
	{
		return;
	}

	const int32 Removed = RegisteredClaims.RemoveAll(
		[Claim](const TWeakObjectPtr<UWorldClaimComponent>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == Claim;
		});

	if (Removed > 0)
	{
		UE_LOG(LogWorldClaims, Verbose, TEXT("Unregistered claim '%s'; %d remain."),
			*GetNameSafe(Claim->GetOwner()), RegisteredClaims.Num());
	}
}

bool UWorldClaimRegistry::PassesTagFilter(const UWorldClaimComponent& Claim,
	const FGameplayTagContainer& RequiredTags, bool bRequireAllTags)
{
	if (RequiredTags.IsEmpty())
	{
		return true;
	}
	return bRequireAllTags ? Claim.ClaimTags.HasAll(RequiredTags)
						   : Claim.ClaimTags.HasAny(RequiredTags);
}

void UWorldClaimRegistry::QueryClaims(const FBox& QueryBounds, const FGameplayTagContainer& RequiredTags,
	bool bRequireAllTags, TArray<UWorldClaimComponent*>& OutClaims) const
{
	OutClaims.Reset();

	for (int32 Index = RegisteredClaims.Num() - 1; Index >= 0; --Index)
	{
		UWorldClaimComponent* Claim = RegisteredClaims[Index].Get();
		if (!Claim)
		{
			// Lazy compaction of stale weak refs during iteration.
			RegisteredClaims.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		if (!PassesTagFilter(*Claim, RequiredTags, bRequireAllTags))
		{
			continue;
		}

		if (Claim->GetClaimBounds().Intersect(QueryBounds))
		{
			OutClaims.Add(Claim);
		}
	}
}

void UWorldClaimRegistry::QueryClaimsInBox(const FVector& Center, const FVector& HalfExtent,
	const FGameplayTagContainer& RequiredTags, bool bRequireAllTags,
	TArray<UWorldClaimComponent*>& OutClaims) const
{
	const FBox Box(Center - HalfExtent, Center + HalfExtent);
	QueryClaims(Box, RequiredTags, bRequireAllTags, OutClaims);
}

UWorldClaimComponent* UWorldClaimRegistry::GetHighestPriorityClaimAt(const FVector& WorldPoint,
	const FGameplayTagContainer& RequiredTags, bool bRequireAllTags) const
{
	UWorldClaimComponent* Best = nullptr;

	for (int32 Index = RegisteredClaims.Num() - 1; Index >= 0; --Index)
	{
		UWorldClaimComponent* Claim = RegisteredClaims[Index].Get();
		if (!Claim)
		{
			RegisteredClaims.RemoveAtSwap(Index, EAllowShrinking::No);
			continue;
		}

		if (!PassesTagFilter(*Claim, RequiredTags, bRequireAllTags))
		{
			continue;
		}

		if (Claim->ContainsPoint(WorldPoint))
		{
			// Strictly-greater keeps the earliest-registered claim on ties.
			if (!Best || Claim->Priority > Best->Priority)
			{
				Best = Claim;
			}
		}
	}

	return Best;
}

void UWorldClaimRegistry::GetAllClaims(TArray<UWorldClaimComponent*>& OutClaims) const
{
	OutClaims.Reset();

	for (int32 Index = RegisteredClaims.Num() - 1; Index >= 0; --Index)
	{
		if (UWorldClaimComponent* Claim = RegisteredClaims[Index].Get())
		{
			OutClaims.Add(Claim);
		}
		else
		{
			RegisteredClaims.RemoveAtSwap(Index, EAllowShrinking::No);
		}
	}
}

int32 UWorldClaimRegistry::GetClaimCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<UWorldClaimComponent>& Entry : RegisteredClaims)
	{
		if (Entry.IsValid())
		{
			++Count;
		}
	}
	return Count;
}
