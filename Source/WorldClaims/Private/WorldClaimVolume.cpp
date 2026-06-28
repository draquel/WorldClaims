// Copyright Daniel Raquel. All Rights Reserved.

#include "WorldClaimVolume.h"
#include "WorldClaimComponent.h"
#include "Components/BoxComponent.h"

AWorldClaimVolume::AWorldClaimVolume()
{
	PrimaryActorTick.bCanEverTick = false;

	ClaimComponent = CreateDefaultSubobject<UWorldClaimComponent>(TEXT("ClaimComponent"));
	RootComponent = ClaimComponent;

#if WITH_EDITORONLY_DATA
	EditorBox = CreateDefaultSubobject<UBoxComponent>(TEXT("EditorBox"));
	EditorBox->SetupAttachment(ClaimComponent);
	EditorBox->SetBoxExtent(ClaimComponent->ClaimExtent, /*bUpdateOverlaps*/ false);
	EditorBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EditorBox->SetCollisionProfileName(TEXT("NoCollision"));
	EditorBox->SetHiddenInGame(true);
	EditorBox->SetIsVisualizationComponent(true);
	EditorBox->ShapeColor = FColor(64, 200, 255);
#endif
}

void AWorldClaimVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITORONLY_DATA
	if (EditorBox && ClaimComponent)
	{
		EditorBox->SetBoxExtent(ClaimComponent->ClaimExtent, /*bUpdateOverlaps*/ false);
	}
#endif
}
