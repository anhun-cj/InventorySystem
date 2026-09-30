#include "Items/Components/Inv_ItemComponent.h"
#include "Net/UnrealNetwork.h"

UInv_ItemComponent::UInv_ItemComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	PickUpMessage = FString(TEXT("E 拾取"));
}

void UInv_ItemComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	
	DOREPLIFETIME(UInv_ItemComponent, ItemManifest);
}

void UInv_ItemComponent::InitItemManifest(FInv_ItemManifest Manifest)
{
	ItemManifest = Manifest;
}

void UInv_ItemComponent::PickedUp()
{
	OnPickedUp();
	GetOwner()->Destroy();
}
