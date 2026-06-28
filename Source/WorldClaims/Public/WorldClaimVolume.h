// Copyright Daniel Raquel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldClaimVolume.generated.h"

class UWorldClaimComponent;
class UBoxComponent;

/**
 * A standalone actor that IS a world claim — drag into a level to author a claim by hand.
 *
 * Wraps a UWorldClaimComponent as its root, plus a box sized to the claim's footprint.
 * The box is hidden and non-colliding in game but kept at runtime so PCG's "Get Actor Data"
 * can read the footprint as actor bounds (boundary-safe consumption); in the editor it doubles
 * as the footprint wireframe. Designers set the tags / priority / extent / display on the claim
 * component; gameplay-spawned claims (POIs, constructions) typically add a UWorldClaimComponent
 * to their own actor instead of using this volume.
 */
UCLASS()
class WORLDCLAIMS_API AWorldClaimVolume : public AActor
{
	GENERATED_BODY()

public:
	AWorldClaimVolume();

	/** The claim this volume represents. */
	UFUNCTION(BlueprintCallable, Category = "World Claim")
	UWorldClaimComponent* GetClaimComponent() const { return ClaimComponent; }

protected:
	// Keeps the editor visualization box in sync with the claim's ClaimExtent.
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Claim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWorldClaimComponent> ClaimComponent;

	/**
	 * Box sized to the claim's ClaimExtent. Hidden + non-colliding in game, but present at
	 * runtime so PCG's "Get Actor Data" reads the claim footprint as actor bounds
	 * (PCGHelpers::GetActorBounds includes non-colliding components). Doubles as the in-editor
	 * footprint wireframe.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "World Claim", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> FootprintBox;
};
