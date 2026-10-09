#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Definitions/Inv_ItemDefinition.h"
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
	DOREPLIFETIME(UInv_ItemComponent, ItemIndex);
}

void UInv_ItemComponent::BeginPlay()
{
	Super::BeginPlay();

	//只有服务器需要从资产生成权威清单，客户端通过 ItemManifest 复制接收
	if (GetOwnerRole() == ROLE_Authority)
	{
		BakeManifestFromDefinition();
	}
}

void UInv_ItemComponent::BakeManifestFromDefinition()
{
	if (IsValid(ItemDefinition))
	{
		ItemDefinition->BuildItemManifest(ItemManifest);
	}
}

void UInv_ItemComponent::InitializeFromDefinition(UInv_ItemDefinition* InItemDefinition)
{
	ItemDefinition = InItemDefinition;
	BakeManifestFromDefinition();
}

void UInv_ItemComponent::InitItemManifest(FInv_ItemManifest Manifest)
{
	ItemManifest = MoveTemp(Manifest);
}

void UInv_ItemComponent::PickedUp()
{
	OnPickedUp();
	GetOwner()->Destroy();
}
