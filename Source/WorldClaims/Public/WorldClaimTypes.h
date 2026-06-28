// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldClaimTypes.generated.h"

class UTexture2D;

/**
 * Presentation data for a claim — consumed by Map and Compass to draw a marker.
 *
 * Pure display metadata; carries no gameplay or spatial semantics (those live on
 * the claim's tags / priority / shape). Optional — a claim with an empty label
 * and null icon simply has no map presence.
 */
USTRUCT(BlueprintType)
struct WORLDCLAIMS_API FWorldClaimDisplayInfo
{
	GENERATED_BODY()

	/** Human-readable name shown on the map / compass (e.g. "Ravenhold"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Claim|Display")
	FText Label;

	/** Marker icon. Soft ref so the map can stream icons without hard-loading every claim. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Claim|Display")
	TSoftObjectPtr<UTexture2D> Icon;

	/** Marker tint / category color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "World Claim|Display")
	FLinearColor Color = FLinearColor::White;

	/** True if there is anything worth drawing for this claim. */
	bool HasDisplay() const
	{
		return !Label.IsEmpty() || !Icon.IsNull();
	}
};
