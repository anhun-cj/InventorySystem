#include "Items/Inv_InventoryItem.h"

#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"


void UInv_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInv_InventoryItem, ItemManifest);
}

void UInv_InventoryItem::SetItemManifest(const FInv_ItemManifest& Manifest)
{
	//ItemManifest = FInstancedStruct::Make<FInv_ItemManifest>(Manifest);
	ItemManifest = TInstancedStruct<FInv_ItemManifest>::Make(Manifest);
}

bool UInv_InventoryItem::IsStackable() const
{
	const FInv_StackableFragment* StackableFragment = GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	return StackableFragment != nullptr;
}

bool UInv_InventoryItem::IsConsumable() const
{
	return GetItemManifest().GetItemCategory() == EInv_ItemCategory::Consumable;
}
