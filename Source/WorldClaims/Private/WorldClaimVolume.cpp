// Copyright Daniel Raquel. All Rights Reserved.

#include "WorldClaimVolume.h"
#include "WorldClaimComponent.h"
#include "Components/BoxComponent.h"

AWorldClaimVolume::AWorldClaimVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	ClaimComponent = CreateDefaultSubobject<UWorldClaimComponent>(TEXT("ClaimComponent"));
	RootComponent = ClaimComponent;

	// Runtime (not editor-only): the footprint bounds source PCG reads via "Get Actor Data".
	// Hidden + non-colliding, so it has no gameplay presence — only contributes actor bounds.
	FootprintBox = CreateDefaultSubobject<UBoxComponent>(TEXT("FootprintBox"));
	FootprintBox->SetupAttachment(ClaimComponent);
	FootprintBox->SetBoxExtent(ClaimComponent->ClaimExtent, /*bUpdateOverlaps*/ false);
	FootprintBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FootprintBox->SetCollisionProfileName(TEXT("NoCollision"));
	FootprintBox->SetGenerateOverlapEvents(false);
	FootprintBox->SetHiddenInGame(true);
	FootprintBox->ShapeColor = FColor(64, 200, 255);
}

void AWorldClaimVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (FootprintBox && ClaimComponent)
	{
		FootprintBox->SetBoxExtent(ClaimComponent->ClaimExtent, /*bUpdateOverlaps*/ false);
	}
}
