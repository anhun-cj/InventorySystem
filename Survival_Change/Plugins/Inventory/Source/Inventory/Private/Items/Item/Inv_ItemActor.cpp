#include "Items/Item/Inv_ItemActor.h"

#include "Interaction/Inv_HighlightableStaticMesh.h"
#include "Items/Components/Inv_ItemComponent.h"


AInv_ItemActor::AInv_ItemActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	ItemComponent = CreateDefaultSubobject<UInv_ItemComponent>("ItemComponent");
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	StaticMeshComponent = CreateDefaultSubobject<UInv_HighlightableStaticMesh>("StaticMeshComponent");
	StaticMeshComponent->SetCollisionProfileName(TEXT("Item"));
	StaticMeshComponent->SetupAttachment(RootComponent);
}

void AInv_ItemActor::BeginPlay()
{
	Super::BeginPlay();
	
}

