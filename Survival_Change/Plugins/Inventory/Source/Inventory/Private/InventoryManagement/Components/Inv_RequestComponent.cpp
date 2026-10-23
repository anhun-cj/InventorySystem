#include "InventoryManagement/Components/Inv_RequestComponent.h"

#include "ChaosBreakingEventFilter.h"
#include "Algo/Count.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "GameFramework/PlayerController.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Inv_ItemTags.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "VT/LightmapVirtualTexture.h"
#include "InventoryManagement/Grid/Inv_GridLayout.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
#include "Misc/ScopeExit.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"

UInv_RequestComponent::UInv_RequestComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInv_RequestComponent::OpenInventory_Implementation(UInv_InventoryComponent* Inventory)
{
	if (!IsValid(Inventory))
		return;
	Client_OpenInventory(Inventory);
}

void UInv_RequestComponent::Client_OpenInventory_Implementation(UInv_InventoryComponent* Inventory)
{
	UInv_InventoryComponent* PlayerInventory = UInv_InventoryStatics::GetInventoryComponent(Cast<APlayerController>(GetOwner()));
	if (IsValid(PlayerInventory))
		PlayerInventory->OtherOpenInventoryMenu(Inventory);
}

void UInv_RequestComponent::ConsumeItem_Implementation(UInv_InventoryItem* Item)
{
	if (!IsValid(Item) || !Item->IsConsumable())
		return;
	UInv_InventoryComponent* InventoryComponent = Item->GetInventoryComponent();
	APlayerController* Consumer = Cast<APlayerController>(GetOwner());
	if (!IsValid(InventoryComponent) || !IsValid(Consumer))
		return;
	UInv_InventoryItem* Container = Item->ParentContainerItem.Get();
	FInv_InventoryFastArray& List = Container ? Container->InventoryList : InventoryComponent->GetInventoryListMutable();
	if (!List.FindItem(Item))
		return;
	FInv_ConsumeFragment* ConsumeFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_ConsumeFragment>();
	if (!ConsumeFragment)
		return;
	const FInv_StackableFragment* Stackable = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	const int32 Count = Stackable ? Stackable->GetStackCount() : 1;
	if (Count <= 0)
		return;

	ConsumeFragment->OnConsume(Consumer);
	if (Count > 1)
	{
		List.ChangeEntryByIndex(Item, Count - 1);
		(Container ? Container->OnItemChanged : InventoryComponent->OnItemChanged).Broadcast(Item);
	}
	else
	{
		InventoryComponent->Server_RemoveItem(Item);
	}
}

void UInv_RequestComponent::SplitItem_Implementation(UInv_InventoryItem* Item, int32 Count, int32 Index, int32 SubGridIndex)
{
	if (!IsValid(Item))
		return;

	UInv_InventoryComponent* InventoryComponent = Item->GetInventoryComponent();
	if (!IsValid(InventoryComponent))
		return;
	UInv_InventoryItem* Container = Item->ParentContainerItem.Get();
	if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(InventoryComponent); NearbyInventory && !Container)
	{
		NearbyInventory->SplitNearbyItem(Item, Count);
		return;
	}
	if (Count <= 0 || Index < 0)
		return;
	FInv_InventoryFastArray& List = Container ? Container->InventoryList : InventoryComponent->GetInventoryListMutable();
	if (!List.FindItem(Item))
		return;

	const FInv_StackableFragment* Stackable = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	if (!Stackable || Count >= Stackable->GetStackCount())
		return;
	const int32 StackCount = Stackable->GetStackCount();

	const UInv_InventoryGridAsset* Asset = Container ? Container->GridAsset.Get() : InventoryComponent->GridAsset.Get();
	if (!Asset->Grids.IsValidIndex(SubGridIndex))
		return;
	TArray<UInv_InventoryItem*> Items;
	for (const FInv_InventoryEntry& Entry : List.GetAllItems())
		if (Entry.SubGridIndex == SubGridIndex)
			Items.Add(Entry.GetItem());
	const FIntPoint Dimensions = FInv_GridLayout::GetItemDimensions(Item->GetItemManifest());
	int32 Rows = Asset->Grids[SubGridIndex]->Row;
	if (!Container && !Cast<APlayerController>(InventoryComponent->GetOwner()))
		Rows = FMath::Max(FInv_GridLayout::GetRequiredRows(Items, Asset->Grids[SubGridIndex]->Col, Rows), Index / Asset->Grids[SubGridIndex]->Col + Dimensions.Y);
	TArray<bool> Occupancy;
	FInv_GridLayout::BuildOccupancy(Items, Asset->Grids[SubGridIndex]->Col, Rows, Occupancy);
	FInv_GridLayout Layout;
	Layout.Setup(Asset->Grids[SubGridIndex]->Col, Rows, Occupancy);
	if (!Layout.CanPlaceAt(Index, Dimensions, {}))
		return;
	UInv_InventoryItem* NewItem = List.AddEntryOnSplit(Item, Index, Count, SubGridIndex);

	List.ChangeEntryByIndex(Item, StackCount - Count);
	(Container ? Container->OnItemChanged : InventoryComponent->OnItemChanged).Broadcast(Item);
	(Container ? Container->OnItemAdded : InventoryComponent->OnItemAdded).Broadcast(NewItem);
}

void UInv_RequestComponent::Multicast_EquipItem_Implementation(UInv_InventoryItem* ItemToEquip,
                                                               UInv_InventoryItem* ItemToUnequip, UInv_InventoryComponent* TargetComponent)
{
	TargetComponent->OnItemUnequipped.Broadcast(ItemToUnequip);
	TargetComponent->OnItemAdded.Broadcast(ItemToEquip);
}

// 服务器处理装备、卸装和换装：核对来源及装备槽类型，确认卸下物品有位置后再修改库存。
void UInv_RequestComponent::Server_EquipItem_Implementation(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip, int32 ItemDropIndex,
	UInv_InventoryComponent* TargetComponent, UInv_InventoryComponent* SourceComponent,
	const FGameplayTag& EquipmentType, int32 SourceIndex, UInv_InventoryItem* SourceContainer, int32 SourceSubGridIndex, int32 TargetSubGridIndex, bool bFindUnequipSpace)
{
	bool bAccepted = false;
	// 任意校验提前返回都通知客户端恢复 UI；成功则由库存复制更新显示。
	ON_SCOPE_EXIT
	{
		if (!bAccepted)
			Client_DragResult();
	};
	if (!IsValid(SourceComponent) || !IsValid(TargetComponent))
		return;
	if (SourceContainer && (!SourceContainer->GridAsset || SourceContainer->GetInventoryComponent() != SourceComponent))
		return;
	const bool bAddToContainer = !ItemToEquip && SourceContainer && ItemDropIndex == INDEX_NONE;
	// 卸下装备拖到容器图标时，由服务器在容器内部查找空位。
	if (bAddToContainer)
	{
		if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(SourceComponent))
		{
			UInv_ItemComponent* Pickup = NearbyInventory->FindItemComponent(SourceContainer);
			if (!IsValid(Pickup))
				return;
			SourceComponent = Pickup->GetOwner()->FindComponentByClass<UInv_InventoryComponent>();
			SourceContainer = nullptr;
		}
	}
	FInv_InventoryFastArray& SourceList = SourceContainer ? SourceContainer->InventoryList : SourceComponent->GetInventoryListMutable();
	if (ItemToEquip)
	{
		if (SourceIndex == INDEX_NONE || ItemToEquip->GetGridIndex() != SourceIndex || ItemToEquip->GetSubGridIndex() != SourceSubGridIndex)
			return;
		if (ItemToEquip->GetInventoryComponent() != SourceComponent
			|| ItemToEquip->ParentContainerItem.Get() != SourceContainer || !SourceList.FindItem(ItemToEquip))
			return;
		if (!ItemToEquip->GetItemManifest().GetItemType().MatchesTag(EquipmentType))
			return;
	}
	if (ItemToUnequip)
	{
		if (ItemToUnequip->GetGridIndex() != INDEX_NONE || !TargetComponent->GetInventoryList().FindItem(ItemToUnequip))
			return;
		if (!ItemToUnequip->GetItemManifest().GetItemType().MatchesTag(EquipmentType))
			return;
	}

	for (const FInv_InventoryEntry& Entry : TargetComponent->GetInventoryList().GetAllItems())
	{
		if (Entry.GridIndex == INDEX_NONE
			&& Entry.GetItem()->GetItemManifest().GetItemType().MatchesTag(EquipmentType)
			&& Entry.GetItem() != ItemToUnequip)
			return;
	}
	UInv_InventoryComponent* UnequipComponent = SourceComponent;
	UInv_InventoryItem* UnequipContainer = SourceContainer;
	if (bFindUnequipSpace && ItemToUnequip)
	{
		// 双击换装依次尝试主库存、已装备背包、弹挂；即将装备的物品不占用空间。
		TArray<UInv_InventoryItem*> Containers{nullptr};
		for (const FInv_InventoryEntry& Entry : TargetComponent->GetInventoryList().GetAllItems())
		{
			UInv_InventoryItem* Container = Entry.GetItem();
			if (Entry.GridIndex != INDEX_NONE || Container == ItemToUnequip || !Container->GridAsset)
				continue;
			if (Container->GetItemManifest().GetItemType().MatchesTag(GameItems::Equipment::BackPacks::BackPack))
				Containers.Insert(Container, 1);
			else if (Container->GetItemManifest().GetItemType().MatchesTag(GameItems::Equipment::ChestRigs::ChestRig))
				Containers.Add(Container);
		}
		ItemDropIndex = INDEX_NONE;
		for (UInv_InventoryItem* Container : Containers)
		{
			const FInv_InventoryFastArray& List = Container ? Container->InventoryList : TargetComponent->GetInventoryList();
			const UInv_InventoryGridAsset* Asset = Container ? Container->GridAsset.Get() : TargetComponent->GridAsset.Get();
			TArray<UInv_InventoryItem*> Items;
			for (const FInv_InventoryEntry& Entry : List.GetAllItems())
				if (Entry.GetItem() != ItemToEquip)
					Items.Add(Entry.GetItem());
			const FInv_SlotAvailabilityResult Space = FInv_GridLayout::QueryInventory(Items, *Asset, ItemToUnequip->GetItemManifest(), INDEX_NONE, false);
			if (Space.SlotAvailabilities.IsEmpty())
				continue;
			UnequipComponent = TargetComponent;
			UnequipContainer = Container;
			ItemDropIndex = Space.SlotAvailabilities[0].Index;
			TargetSubGridIndex = Space.SlotAvailabilities[0].SubGridIndex;
			break;
		}
		if (ItemDropIndex == INDEX_NONE)
			return;
	}
	FInv_InventoryFastArray& UnequipList = UnequipContainer ? UnequipContainer->InventoryList : UnequipComponent->GetInventoryListMutable();
	if (ItemToUnequip && !bFindUnequipSpace)
	{
		for (UInv_InventoryItem* Parent = UnequipContainer; Parent; Parent = Parent->ParentContainerItem.Get())
		{
			if (Parent == ItemToUnequip)
				return;
		}
		const FIntPoint Dimensions = FInv_GridLayout::GetItemDimensions(ItemToUnequip->GetItemManifest());
		const UInv_InventoryGridAsset* Asset = UnequipContainer ? UnequipContainer->GridAsset.Get() : UnequipComponent->GridAsset.Get();
		if (!Asset->Grids.IsValidIndex(TargetSubGridIndex))
			return;
		FIntPoint GridSize(Asset->Grids[TargetSubGridIndex]->Col, Asset->Grids[TargetSubGridIndex]->Row);
		if (!UnequipContainer && (!Cast<APlayerController>(UnequipComponent->GetOwner()) || Cast<UInv_FindItemComponent>(UnequipComponent)))
			GridSize.Y = FMath::Max(GridSize.Y, ItemDropIndex / GridSize.X + Dimensions.Y);
		TArray<UInv_InventoryItem*> Items;
		for (const FInv_InventoryEntry& Entry : UnequipList.GetAllItems())
		{
			if (Entry.GetItem() != ItemToEquip && Entry.SubGridIndex == TargetSubGridIndex)
				Items.Add(Entry.GetItem());
		}
		// 换装先忽略即将穿上的物品，再按卸下装备自己的尺寸检查空位。
		if (bAddToContainer)
		{
			Items.Reset();
			for (const FInv_InventoryEntry& Entry : UnequipList.GetAllItems())
				Items.Add(Entry.GetItem());
			const FInv_SlotAvailabilityResult Space = FInv_GridLayout::QueryInventory(Items, *Asset, ItemToUnequip->GetItemManifest(), INDEX_NONE, false);
			if (Space.SlotAvailabilities.IsEmpty())
				return;
			TargetSubGridIndex = Space.SlotAvailabilities[0].SubGridIndex;
			ItemDropIndex = Space.SlotAvailabilities[0].Index;
			GridSize = FIntPoint(Asset->Grids[TargetSubGridIndex]->Col, Asset->Grids[TargetSubGridIndex]->Row);
		}
		TArray<bool> Occupancy;
		FInv_GridLayout::BuildOccupancy(Items, GridSize.X, GridSize.Y, Occupancy, TargetSubGridIndex);
		FInv_GridLayout Layout;
		Layout.Setup(GridSize.X, GridSize.Y, Occupancy);
		if (!Layout.CanPlaceAt(ItemDropIndex, Dimensions, TSet<int32>()))
			return;
	}
	if (ItemToUnequip)
		TargetComponent->Server_RemoveItem(ItemToUnequip);
	if (ItemToEquip)
	{
		SourceComponent->Server_RemoveItem(ItemToEquip);
		ItemToEquip->SetInventoryComponent(TargetComponent);
		TargetComponent->GetInventoryListMutable().AddEntryOnEquip(ItemToEquip);
		TargetComponent->OnItemEquipped.Broadcast(ItemToEquip);
	}
	if (ItemToUnequip)
	{
		ItemToUnequip->SetInventoryComponent(UnequipComponent);
		UnequipList.AddEntry(ItemToUnequip, ItemDropIndex, TargetSubGridIndex);
		(UnequipContainer ? UnequipContainer->OnItemAdded : UnequipComponent->OnItemAdded).Broadcast(ItemToUnequip);
	}
	bAccepted = true;
}

void UInv_RequestComponent::DropItem_Implementation(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	UInv_InventoryComponent* InventoryComponent = Item->GetInventoryComponent();
	if (!IsValid(InventoryComponent))
		return;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	InventoryComponent->DropItemForPawn(Item, Controller->GetPawn());
}

// 普通拖拽提交指定落点；拖到容器图标则由服务器计算内部堆叠和空位。
void UInv_RequestComponent::TransferItem_Implementation(UInv_InventoryItem* SourceItem,
	const FInv_SpaceQueryResult& Result, int32 DropIndex, UInv_InventoryComponent* TargetComponent,
	UInv_InventoryComponent* SourceComponent, int32 SourceIndex, UInv_InventoryItem* TargetContainer, UInv_InventoryItem* SourceContainer, int32 SourceSubGridIndex, int32 TargetSubGridIndex)
{
	bool bAccepted = false;
	ON_SCOPE_EXIT
	{
		if (!bAccepted)
			Client_DragResult();
	};
	if (!IsValid(SourceItem) || !IsValid(SourceComponent) || !IsValid(TargetComponent))
		return;
	const bool bAddToContainer = TargetContainer && DropIndex == INDEX_NONE;
	if ((!bAddToContainer && !Result.bHasSpace && !Result.bAllowItemDown) || SourceIndex == INDEX_NONE)
		return;
	if (SourceItem->GetGridIndex() != SourceIndex || SourceItem->GetSubGridIndex() != SourceSubGridIndex || SourceItem->GetInventoryComponent() != SourceComponent
		|| SourceItem->ParentContainerItem.Get() != SourceContainer)
		return;
	if (TargetContainer && (!TargetContainer->GridAsset || TargetContainer->GetInventoryComponent() != TargetComponent))
		return;
	if (bAddToContainer)
	{
		// 沿父容器向上检查，禁止把容器放进自身或自己的后代容器。
		for (UInv_InventoryItem* Parent = TargetContainer; Parent; Parent = Parent->ParentContainerItem.Get())
			if (Parent == SourceItem)
				return;
		if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(TargetComponent))
		{
			// 附近列表只是代理，实际存入地面 Actor 的库存组件。
			UInv_ItemComponent* Pickup = NearbyInventory->FindItemComponent(TargetContainer);
			if (!IsValid(Pickup))
				return;
			TargetComponent = Pickup->GetOwner()->FindComponentByClass<UInv_InventoryComponent>();
			TargetContainer = nullptr;
		}
		if (TargetComponent == SourceComponent && TargetContainer == SourceContainer)
			return;
		FInv_InventoryFastArray& TargetList = TargetContainer ? TargetContainer->InventoryList : TargetComponent->GetInventoryListMutable();
		const UInv_InventoryGridAsset* Asset = TargetContainer ? TargetContainer->GridAsset.Get() : TargetComponent->GridAsset.Get();
		TArray<UInv_InventoryItem*> Items;
		for (const FInv_InventoryEntry& Entry : TargetList.GetAllItems())
			if (Entry.GridIndex != INDEX_NONE)
			{
				Items.Add(Entry.GetItem());
			}
		const FInv_SlotAvailabilityResult Space = FInv_GridLayout::QueryInventory(Items, *Asset, SourceItem->GetItemManifest(), INDEX_NONE, true);
		FInv_InventoryFastArray& SourceList = SourceContainer ? SourceContainer->InventoryList : SourceComponent->GetInventoryListMutable();
		for (const FInv_SlotAvailability& Slot : Space.SlotAvailabilities)
		{
			FInv_SpaceQueryResult Preview;
			Preview.bHasSpace = !Slot.bItemAtIndex;
			Preview.TargetSubGridIndex = Slot.SubGridIndex;
			Preview.bAllowItemDown = true;
			if (Slot.bItemAtIndex)
			{
				Preview.ValidItems.Add(Slot.Item);
				Preview.ItemMoveIndices.Add(SourceIndex);
			}
			if (!TargetComponent->MoveItemToThisInventory(*SourceItem, Preview, Slot.Index, TargetContainer))
				break;
			bAccepted = true;
			if (!SourceList.FindItem(SourceItem))
				break;
		}
		return;
	}
	FInv_SpaceQueryResult Preview = Result;
	Preview.TargetSubGridIndex = TargetSubGridIndex;
	bAccepted = TargetComponent->MoveItemToThisInventory(*SourceItem, Preview, DropIndex, TargetContainer);
}

// 服务器拒绝操作时，只通知 UI 根据客户端当前复制到的库存数据重新生成显示。
void UInv_RequestComponent::Client_DragResult_Implementation()
{
	UInv_InventoryComponent* Inventory = UInv_InventoryStatics::GetInventoryComponent(Cast<APlayerController>(GetOwner()));
	if (UInv_SpatialInventory* Menu = Cast<UInv_SpatialInventory>(Inventory->GetInventoryMenu()))
		Menu->FinishDragPrediction();
}
