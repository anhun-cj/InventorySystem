#include "Items/Inv_InventoryItem.h"

#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"


void UInv_InventoryItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInv_InventoryItem, ItemManifest);
	DOREPLIFETIME(UInv_InventoryItem, ItemIndex);
	DOREPLIFETIME(UInv_InventoryItem, GridIndex);
	DOREPLIFETIME(UInv_InventoryItem, SubGridIndex);
	DOREPLIFETIME(UInv_InventoryItem, OldGridIndex);
	DOREPLIFETIME(UInv_InventoryItem, OwnerInventory);
	DOREPLIFETIME(UInv_InventoryItem, OldOwnerInventory);
	DOREPLIFETIME(UInv_InventoryItem, ParentContainerItem);
	DOREPLIFETIME(UInv_InventoryItem, InventoryList);
	DOREPLIFETIME(UInv_InventoryItem, GridAsset);
	DOREPLIFETIME(UInv_InventoryItem, GroundInventory);
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

// 更新负责管理、复制物品的组件，并将新归属传给内部所有子物品。
void UInv_InventoryItem::SetInventoryComponent(UInv_InventoryComponent* InventoryComponent)
{
	OldOwnerInventory = OwnerInventory;
	// 同一归属可能被重复设置；只有真正转移时才清除附近条目的地面引用。
	if (OwnerInventory.Get() != InventoryComponent)
		GroundInventory.Reset();
	OwnerInventory = InventoryComponent;
	for (const FInv_InventoryEntry& Entry : InventoryList.GetAllItems())
		Entry.GetItem()->SetInventoryComponent(InventoryComponent);
}
