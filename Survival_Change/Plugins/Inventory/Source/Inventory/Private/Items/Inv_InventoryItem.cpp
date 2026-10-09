#include "Items/Inv_InventoryItem.h"

#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"


void UInv_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInv_InventoryItem, ItemManifest);
	DOREPLIFETIME(UInv_InventoryItem, ItemIndex);
	DOREPLIFETIME(UInv_InventoryItem, GridIndex);
	DOREPLIFETIME(UInv_InventoryItem, OldGridIndex);
	DOREPLIFETIME(UInv_InventoryItem, OwnerInventory);
	DOREPLIFETIME(UInv_InventoryItem, OldOwningInventoryComponent);
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

UInv_InventoryComponent* UInv_InventoryItem::GetInventoryComponent() const
{
	return OwnerInventory.Get();
}

void UInv_InventoryItem::SetInventoryComponent(UInv_InventoryComponent* InventoryComponent)
{
	OldOwningInventoryComponent = OwnerInventory;
	OwnerInventory = InventoryComponent;
}

UInv_InventoryComponent* UInv_InventoryItem::GetOldInventoryComponent() const
{
	return OldOwningInventoryComponent.Get();
}
