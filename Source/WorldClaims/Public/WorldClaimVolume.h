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
 * Wraps a UWorldClaimComponent as its root, plus an editor-only box for in-viewport
 * visualization of the footprint. Designers set the tags / priority / extent / display on
 * the claim component; gameplay-spawned claims (POIs, constructions) typically add a
 * UWorldClaimComponent to their own actor instead of using this volume.
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

#if WITH_EDITORONLY_DATA
	/** Wireframe box drawn in-editor to show the claim footprint; stripped from game/packaged builds. */
	UPROPERTY()
	TObjectPtr<UBoxComponent> EditorBox;
#endif
};
