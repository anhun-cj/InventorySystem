#include "Building/Inv_Building.h"

#include "Interaction/Inv_HighlightableStaticMesh.h"


AInv_Building::AInv_Building()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	StaticMesh = CreateDefaultSubobject<UInv_HighlightableStaticMesh>("StaticMesh");
	StaticMesh->SetupAttachment(RootComponent);
	StaticMesh->SetCollisionProfileName(TEXT("Item"));
}

void AInv_Building::BeginPlay()
{
	Super::BeginPlay();
	
}

FString AInv_Building::GetOpenMessage() const
{
	return OpenMessage;
}

FString AInv_Building::GetBuildingName() const
{
	return Name;
}

TArray<FText> AInv_Building::GetMessages() const
{
	return Messages;
}

