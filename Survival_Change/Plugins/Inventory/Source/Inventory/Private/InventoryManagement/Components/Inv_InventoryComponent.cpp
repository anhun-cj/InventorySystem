#include "InventoryManagement/Components/Inv_InventoryComponent.h"

#include "Items/Components/Inv_ItemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Grid/Inv_GridLayout.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
#include "Engine/World.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Inv_ItemTags.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"
#include "Widget/Utils/Inv_WidgetUtils.h"
#include "Widget/HUD/Inv_HUDWidget.h"

namespace
{
	FIntRect GetItemRect(const UInv_InventoryItem& Item, int32 Index, int32 Columns)
	{
		const FIntPoint Position(Index % Columns, Index / Columns);
		return FIntRect(Position, Position + FInv_GridLayout::GetItemDimensions(Item.GetItemManifest()));
	}

	bool CanPlaceItem(const UInv_InventoryItem& Item, int32 Index, const UInv_InventoryComponent& Inventory, FIntRect& Rect, const UInv_InventoryItem* ContainerItem = nullptr, int32 SubGridIndex = 0)
	{
		if (Index < 0)
			return false;
		const UInv_InventoryGridAsset* Asset = ContainerItem ? ContainerItem->GridAsset.Get() : Inventory.GridAsset.Get();
		Rect = GetItemRect(Item, Index, Asset->Grids[SubGridIndex]->Col);
		const bool bCanGrow = !ContainerItem && (!Cast<APlayerController>(Inventory.GetOwner()) || Cast<UInv_FindItemComponent>(&Inventory));
		return Rect.Max.X <= Asset->Grids[SubGridIndex]->Col && (bCanGrow || Rect.Max.Y <= Asset->Grids[SubGridIndex]->Row);
	}
}

UInv_InventoryComponent::UInv_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}

void UInv_InventoryComponent::MoveItemOnServer(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result,
	int32 DropIndex)
{
	Server_MoveItem(SourceItem, Result, DropIndex);
}

void UInv_InventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInv_InventoryComponent, InventoryList);
}

void UInv_InventoryComponent::TryAddItem(UInv_ItemComponent* ItemComponent)
{
	Server_PickupItem(ItemComponent);
}

bool UInv_InventoryComponent::AddItem(UInv_InventoryItem* Item, int32& GridIndex, UInv_InventoryGrid* Grid, int32* OutSubGridIndex) const
{
	if (!Grid)
		Grid = BoundInventoryGrid.Get();
	
	Grid = Grid->GetSubGrid(INDEX_NONE);
	
	const FInv_SlotAvailabilityResult Result = Grid->HasRoomForItem(Item, -1, false);
	
	if (Result.SlotAvailabilities.IsEmpty())
		return false;
	
	GridIndex = Result.SlotAvailabilities[0].Index;
	if (OutSubGridIndex)
		*OutSubGridIndex = Result.SlotAvailabilities[0].SubGridIndex;
	Grid->GetSubGrid(Result.SlotAvailabilities[0].SubGridIndex)->AddItemToIndices(Item, GridIndex);
	return true;
}

void UInv_InventoryComponent::SetBoundInventoryGrid(UInv_InventoryGrid* Grid)
{
	BoundInventoryGrid = Grid;
}

// 装备优先穿到空槽；需要存放时，由服务器依次查询主库存、已装备背包、其他已装备容器。
void UInv_InventoryComponent::Server_PickupItem_Implementation(UInv_ItemComponent* ItemComponent)
{
	if (Cast<UInv_FindItemComponent>(this) || !IsValid(ItemComponent) || !IsValid(ItemComponent->GetOwner())
		|| ItemComponent->GetOwner()->IsActorBeingDestroyed() || ItemComponent->GetWorld() != GetWorld())
		return;
	
	const FInv_ItemManifest Manifest = ItemComponent->GetItemManifest();
	const FInv_StackableFragment* Stackable = Manifest.GetFragmentOfType<FInv_StackableFragment>();
	const int32 SourceCount = Stackable ? Stackable->GetStackCount() : 1;
	if (SourceCount <= 0)
		return;
	// 角色拾取装备时优先使用空装备槽，不占用普通库存空间，也不替换已穿装备。
	if (Cast<APlayerController>(GetOwner()))
	{
		if (Manifest.GetFragmentOfType<FInv_EquipmentFragment>())
		{
			// 按物品类型确定装备槽；装备片段的挂接标签不参与槽位占用判断。
			const FGameplayTag EquipmentTypes[] = {
				GameItems::Equipment::Weapons::Weapon,
				GameItems::Equipment::BackPacks::BackPack,
				GameItems::Equipment::ChestRigs::ChestRig,
				GameItems::Equipment::Helmets::Helmet,
				GameItems::Equipment::ChestArmors::ChestArmor,
				GameItems::Equipment::Pistols::Pistol,
				GameItems::Equipment::Knifes::Knife
			};
			for (const FGameplayTag& EquipmentType : EquipmentTypes)
			{
				if (!Manifest.GetItemType().MatchesTag(EquipmentType))
					continue;
				const bool bOccupied = InventoryList.GetAllItems().ContainsByPredicate([&EquipmentType](const FInv_InventoryEntry& Entry)
				{
					return Entry.GridIndex == INDEX_NONE
						&& Entry.GetItem()->GetItemManifest().GetItemType().MatchesTag(EquipmentType);
				});
				if (!bOccupied)
				{
					UInv_InventoryItem* Item = InventoryList.AddEntry(ItemComponent, 1, INDEX_NONE);
					OnItemEquipped.Broadcast(Item);
					UpdatePickupActor(ItemComponent, 0);
					return;
				}
				break;
			}
		}
	}
	//容器排序，没有装备的容器除外()优先添加顺序
	//容器初始化长度为1，对象为nullptr
	TArray<UInv_InventoryItem*> Containers{nullptr};
	for (const FInv_InventoryEntry& Entry : InventoryList.GetAllItems())
	{
		if (Entry.GridIndex == INDEX_NONE && Entry.GetItem()->GridAsset)
		{
			if (Entry.GetItem()->GetItemManifest().GetItemType().MatchesTag(GameItems::Equipment::BackPacks::BackPack))
				Containers.Insert(Entry.GetItem(), 1);
			else
				Containers.Add(Entry.GetItem());
		}
	}
	int32 Remainder = SourceCount;
	//服务器计算物品存放的库存，存放位置
	for (UInv_InventoryItem* Container : Containers)
	{
		FInv_InventoryFastArray& List = Container ? Container->InventoryList : InventoryList;
		const UInv_InventoryGridAsset* Asset = Container ? Container->GridAsset.Get() : GridAsset.Get();
		TArray<UInv_InventoryItem*> ExistingItems;
		for (const FInv_InventoryEntry& Entry : List.GetAllItems())
		{
			ExistingItems.Add(Entry.GetItem());
		}
		const FInv_SlotAvailabilityResult Result = FInv_GridLayout::QueryInventory(ExistingItems, *Asset, Manifest, Remainder, true);
		for (const FInv_SlotAvailability& Slot : Result.SlotAvailabilities)
		{
			if (Slot.bItemAtIndex)
			{
				UInv_InventoryItem* Existing = Slot.Item.Get();
				const FInv_StackableFragment* ExistingStack = Existing->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
				List.ChangeEntryByIndex(Existing, ExistingStack->GetStackCount() + Slot.AmountToFill);
				(Container ? Container->OnItemChanged : OnItemChanged).Broadcast(Existing);
			}
			else
			{
				UInv_InventoryItem* NewItem = List.AddEntry(ItemComponent, Slot.AmountToFill, Slot.Index, Slot.SubGridIndex);
				(Container ? Container->OnItemAdded : OnItemAdded).Broadcast(NewItem);
			}
			Remainder -= Slot.AmountToFill;
		}
		if (Remainder == 0)
			break;
	}
	if (Remainder < SourceCount)
		UpdatePickupActor(ItemComponent, Remainder);
	else
		Client_NoRoomInInventory();
}

void UInv_InventoryComponent::Client_NoRoomInInventory_Implementation()
{
	NoRoomInInventory.Broadcast();
}

void UInv_InventoryComponent::Multicast_RemoveSlottedItem_Implementation(UInv_InventoryItem* SourceItem)
{
	if (!IsValid(SourceItem))
		return;
	OnRemoveSlottedItem.Broadcast(SourceItem, SourceItem->GetGridIndex());
}

void UInv_InventoryComponent::Server_RemoveSlottedItem_Implementation(UInv_InventoryItem* SourceItem)
{
	Multicast_RemoveSlottedItem(SourceItem);
}

void UInv_InventoryComponent::ChangeItemIndex(UInv_InventoryItem* Item, int32 Index)
{
	Item->SetGridIndex(Index);
	InventoryList.ChangeEntryIndex(Item);
	Item->SetInventoryComponent(this);
	OnItemChanged.Broadcast(Item);
}

void UInv_InventoryComponent::Server_AddInventoryItem_Implementation(const int32 GridIndex, UInv_InventoryItem* Item, int32 StackCount, bool IsInGridSlots)
{
	if (IsInGridSlots)
	{
		InventoryList.ChangeEntryByIndex(Item, StackCount);
		Server_RemoveSlottedItem(Item);
		Item->SetInventoryComponent(this);
		OnItemChanged.Broadcast(Item);
		return;
	}
	UInv_InventoryItem* NewItem =  InventoryList.AddEntry(Item, GridIndex);
	NewItem->SetInventoryComponent(this);
	OnItemAdded.Broadcast(NewItem);
}

void UInv_InventoryComponent::Server_MoveItem_Implementation(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex)
{
	if (!IsValid(SourceItem) || SourceItem->GetInventoryComponent() != this)
		return;
	if ((!Result.bHasSpace && !Result.bAllowItemDown) || SourceItem->GetGridIndex() == INDEX_NONE)
		return;
	MoveItemToThisInventory(*SourceItem, Result, DropIndex);
}

// 服务器执行移动、堆叠或交换；先验证全部落点，校验通过后才允许放置。
bool UInv_InventoryComponent::MoveItemToThisInventory(UInv_InventoryItem& SourceItem,
	const FInv_SpaceQueryResult& Preview, int32 DropIndex, UInv_InventoryItem* TargetContainer)
{
	UInv_InventoryComponent* SourceInventory = SourceItem.GetInventoryComponent();
	UInv_InventoryItem* SourceContainer = SourceItem.ParentContainerItem.Get();
	FInv_InventoryFastArray& SourceList = SourceContainer ? SourceContainer->InventoryList : SourceInventory->InventoryList;
	if (!SourceList.FindItem(&SourceItem))
		return false;
	if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(SourceInventory); NearbyInventory && SourceItem.GridAsset)
	{
		UInv_ItemComponent* Pickup = NearbyInventory->FindItemComponent(&SourceItem);
		if (!IsValid(Pickup) || Pickup->GetOwner() == GetOwner())
			return false;
	}
	FInv_InventoryFastArray& TargetList = TargetContainer ? TargetContainer->InventoryList : InventoryList;
	const UInv_InventoryGridAsset* TargetAsset = TargetContainer ? TargetContainer->GridAsset.Get() : GridAsset.Get();
	if (!TargetAsset->Grids.IsValidIndex(Preview.TargetSubGridIndex))
		return false;

	FIntRect DropRect;
	if (!CanPlaceItem(SourceItem, DropIndex, *this, DropRect, TargetContainer, Preview.TargetSubGridIndex))
		return false;
	
	const int32 Columns = TargetAsset->Grids[Preview.TargetSubGridIndex]->Col;
	// 从真实目标库存收集落点覆盖的物品，核对客户端预览与服务器状态是否一致。
	TArray<UInv_InventoryItem*> Occupants;
	for (const FInv_InventoryEntry& Entry : TargetList.GetAllItems())
	{
		UInv_InventoryItem* Item = Entry.GetItem();
		if (Item == &SourceItem || Item->GetGridIndex() == INDEX_NONE || Item->GetSubGridIndex() != Preview.TargetSubGridIndex)
			continue;
		const FIntRect Rect = GetItemRect(*Item, Item->GetGridIndex(), Columns);
		if (DropRect.Intersect(Rect))
		{
			if (!DropRect.Contains(Rect) || !Preview.ValidItems.Contains(Item))
				return false;
			Occupants.Add(Item);
		}
	}
	if (Preview.bHasSpace ? !Occupants.IsEmpty()
		: Occupants.IsEmpty() || Occupants.Num() != Preview.ValidItems.Num()
			|| Occupants.Num() != Preview.ItemMoveIndices.Num())
		return false;

	UInv_InventoryItem* MergeTarget = Occupants.Num() == 1 ? Occupants[0] : nullptr;
	// 同类可堆叠物品优先合并；有剩余则更新来源数量，全部合并后移除来源条目。
	const FInv_StackableFragment* TargetStack = MergeTarget
		? MergeTarget->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>() : nullptr;
	const FInv_StackableFragment* SourceStack = SourceItem.GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	if (SourceStack && TargetStack
		&& FInv_ItemManifest::AreSameItem(SourceItem.GetItemManifest(), MergeTarget->GetItemManifest())
		&& TargetStack->GetStackCount() < TargetStack->GetMaxStackCount())
	{
		const int32 Amount = FMath::Min(SourceStack->GetStackCount(), TargetStack->GetMaxStackCount() - TargetStack->GetStackCount());
		const int32 Remainder = SourceStack->GetStackCount() - Amount;
		TargetList.ChangeEntryByIndex(MergeTarget, TargetStack->GetStackCount() + Amount);
		if (Remainder > 0)
		{
			SourceList.ChangeEntryByIndex(&SourceItem, Remainder);
			(SourceContainer ? SourceContainer->OnItemChanged : SourceInventory->OnItemChanged).Broadcast(&SourceItem);
		}
		else
		{
			SourceInventory->Server_RemoveItem(&SourceItem);
		}
		(TargetContainer ? TargetContainer->OnItemChanged : OnItemChanged).Broadcast(MergeTarget);
		return true;
	}

	// 先记录拖拽物品及所有被交换物品的最终库存、父容器和格位。
	struct FPlacement
	{
		UInv_InventoryItem* Item;
		UInv_InventoryComponent* Inventory;
		UInv_InventoryItem* Container;
		int32 Index;
		FIntRect Rect;
		int32 SubGridIndex;
	};
	TArray<FPlacement> Placements;
	Placements.Add({&SourceItem, this, TargetContainer, DropIndex, DropRect, Preview.TargetSubGridIndex});
	for (UInv_InventoryItem* Item : Occupants)
	{
		const int32 Index = Preview.ItemMoveIndices[Preview.ValidItems.IndexOfByKey(Item)];
		FIntRect Rect;
		if (!CanPlaceItem(*Item, Index, *SourceInventory, Rect, SourceContainer, SourceItem.GetSubGridIndex()))
			return false;
		Placements.Add({Item, SourceInventory, SourceContainer, Index, Rect, SourceItem.GetSubGridIndex()});
	}
	
	for (int32 i = 0; i < Placements.Num(); ++i)
	{
		// 按交换完成后的父子关系检查容器循环，再检查最终落点是否相互覆盖。
		const FPlacement& Placement = Placements[i];
		TSet<UInv_InventoryItem*> Parents;
		for (UInv_InventoryItem* Parent = Placement.Container; Parent;)
		{
			if (Parent == Placement.Item || Parents.Contains(Parent))
				return false;
			Parents.Add(Parent);
			const FPlacement* ParentPlacement = Placements.FindByPredicate([Parent](const FPlacement& Candidate)
			{
				return Candidate.Item == Parent;
			});
			Parent = ParentPlacement ? ParentPlacement->Container : Parent->ParentContainerItem.Get();
		}
		for (int32 j = 0; j < i; ++j)
			if (Placement.Inventory == Placements[j].Inventory && Placement.Container == Placements[j].Container && Placement.SubGridIndex == Placements[j].SubGridIndex
				&& Placement.Rect.Intersect(Placements[j].Rect))
				return false;
		const UInv_InventoryGridAsset* PlacementAsset = Placement.Container ? Placement.Container->GridAsset.Get() : Placement.Inventory->GridAsset.Get();
		const int32 PlacementColumns = PlacementAsset->Grids[Placement.SubGridIndex]->Col;
		const FInv_InventoryFastArray& List = Placement.Container ? Placement.Container->InventoryList : Placement.Inventory->InventoryList;
		for (const FInv_InventoryEntry& Entry : List.GetAllItems())
		{
			UInv_InventoryItem* Item = Entry.GetItem();
			if (Item == &SourceItem || Occupants.Contains(Item) || Item->GetGridIndex() == INDEX_NONE || Item->GetSubGridIndex() != Placement.SubGridIndex)
				continue;
			if (Placement.Rect.Intersect(GetItemRect(*Item, Item->GetGridIndex(), PlacementColumns)))
				return false;
		}
	}

	if (&SourceList == &TargetList)
	{
		// 同一数组内只改索引；跨数组转移才需要移除旧条目并添加新条目。
		for (const FPlacement& Placement : Placements)
		{
			Placement.Item->SetGridIndex(Placement.Index);
			Placement.Item->SetSubGridIndex(Placement.SubGridIndex);
			TargetList.ChangeEntryIndex(Placement.Item);
		}
		for (const FPlacement& Placement : Placements)
			(TargetContainer ? TargetContainer->OnItemChanged : OnItemChanged).Broadcast(Placement.Item);
	}
	else
	{
		for (const FPlacement& Placement : Placements)
			Placement.Item->GetInventoryComponent()->Server_RemoveItem(Placement.Item);
		for (const FPlacement& Placement : Placements)
		{
			Placement.Item->SetInventoryComponent(Placement.Inventory);
			FInv_InventoryFastArray& List = Placement.Container ? Placement.Container->InventoryList : Placement.Inventory->InventoryList;
			List.AddEntry(Placement.Item, Placement.Index, Placement.SubGridIndex);
		}
		for (const FPlacement& Placement : Placements)
			(Placement.Container ? Placement.Container->OnItemAdded : Placement.Inventory->OnItemAdded).Broadcast(Placement.Item);
	}
	return true;
}

void UInv_InventoryComponent::UpdatePickupActor(UInv_ItemComponent* ItemComponent, int32 Remainder)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !IsValid(ItemComponent) || Remainder < 0)
		return;
	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment = ItemComponent->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
		ItemComponent->OnDataChanged.Broadcast(ItemComponent);
	}
}

void UInv_InventoryComponent::Server_DropItem_Implementation(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* DropPawn = Cast<APawn>(GetOwner());
	if (IsValid(Controller))
		DropPawn = Controller->GetPawn();
	DropItemForPawn(Item, DropPawn);
}

bool UInv_InventoryComponent::DropItemForPawn(UInv_InventoryItem* Item, const APawn* DropPawn)
{
	if (Cast<UInv_FindItemComponent>(this) || Item->GetInventoryComponent() != this)
		return false;
	const FInv_InventoryFastArray& List = Item->ParentContainerItem.IsValid() ? Item->ParentContainerItem->InventoryList : InventoryList;
	if (!List.FindItem(Item))
		return false;
	if (!IsValid(SpawnDroppedItem(Item, DropPawn)))
		return false;
	Server_RemoveItem(Item);
	return true;
}

// 生成地面拾取 Actor；如果丢弃的是容器，将内部内容交给新 Actor 的库存。
UInv_ItemComponent* UInv_InventoryComponent::SpawnDroppedItem(UInv_InventoryItem* Item, const APawn* DropPawn) const
{
	const APawn* OwningPawn = DropPawn;
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()); !IsValid(OwningPawn) && PC)
		OwningPawn = PC->GetPawn();
	if (!IsValid(OwningPawn))
		OwningPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwningPawn) || OwningPawn->GetWorld() != GetWorld())
	{
		return nullptr;
	}

	const FInv_ItemManifest& ItemManifest = Item->GetItemManifest();
	UInv_ItemComponent* Pickup = ItemManifest.SpawnPickUpActor(this, OwningPawn->GetActorLocation(), FRotator::ZeroRotator);
	if (!IsValid(Pickup))
		return nullptr;
	AActor* SpawnedItem = Pickup->GetOwner();
	if (Item->GridAsset)
	{
		Pickup->InventoryGridAsset = Item->GridAsset;
		UInv_InventoryComponent* DroppedInventory = SpawnedItem->FindComponentByClass<UInv_InventoryComponent>();
		DroppedInventory->GridAsset = Item->GridAsset;
		const TArray<FInv_InventoryEntry> Contents = Item->InventoryList.GetAllItems();
		// 先解除旧组件对子物品的复制管理，再按原格位加入地面库存。
		for (const FInv_InventoryEntry& Entry : Contents)
			Item->GetInventoryComponent()->RemoveRepSubObj(Entry.GetItem());
		Item->InventoryList.ClearAllItems();
		for (const FInv_InventoryEntry& Entry : Contents)
		{
			UInv_InventoryItem* ContentItem = Entry.GetItem();
			ContentItem->SetInventoryComponent(DroppedInventory);
			DroppedInventory->InventoryList.AddEntry(ContentItem, Entry.GridIndex, Entry.SubGridIndex);
			DroppedInventory->OnItemAdded.Broadcast(ContentItem);
		}
	}
	SpawnedItem->SetActorLocation(GetDropSpawnLocation(OwningPawn),
		false, nullptr, ETeleportType::TeleportPhysics);
	return Pickup;
}

// 在角色朝向左右各 60 度范围内随机生成位置；仅计算坐标，不做落地或碰撞检测。
FVector UInv_InventoryComponent::GetDropSpawnLocation(const APawn* Pawn) const
{
	const float Yaw = Pawn->GetActorRotation().Yaw + FMath::FRandRange(-60.0f, 60.0f);
	const float MinDistance = FMath::Max(0.0f, DropSpawnDistanceMin);
	const float MaxDistance = FMath::Max(MinDistance, DropSpawnDistanceMax);
	FVector Location = Pawn->GetActorLocation()
		+ FRotator(0.0f, Yaw, 0.0f).Vector() * FMath::FRandRange(MinDistance, MaxDistance);
	Location.Z += RelativeSpawnElevation;
	return Location;
}

UInv_InventoryItem* UInv_InventoryComponent::FindNoFillItem(const FInv_ItemManifest& Source, const TSet<UInv_InventoryItem*>& Items) const
{
	return InventoryList.FindNoFillItemLike(Source, Items);
}

void UInv_InventoryComponent::Server_ConsumeItem_Implementation(UInv_InventoryItem* Item, int32 GridIndex)
{
	if (FInv_ConsumeFragment* ConsumeFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_ConsumeFragment>())
	{
		ConsumeFragment->OnConsume(OwningController.Get());
	}
}

void UInv_InventoryComponent::Server_EquipItem_Implementation(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip, int32 ItemDropIndex, bool bAllowEquip)
{
	if (!bAllowEquip)
		return;
	if (IsValid(ItemToEquip))
	{
		Server_RemoveSlottedItem(ItemToEquip);
		InventoryList.ChangeEntryOnEquip(ItemToEquip);
		OnItemEquipped.Broadcast(ItemToEquip);
	}
	if (IsValid(ItemToUnequip))
	{
		ItemToUnequip->SetGridIndex(ItemDropIndex);
		InventoryList.ChangeEntryIndex(ItemToUnequip);
		OnItemChanged.Broadcast(ItemToUnequip);
	}
	//Multicast_EquipItem(ItemToEquip, ItemToUnequip);
}

void UInv_InventoryComponent::Multicast_EquipItem_Implementation(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip)
{
	OnItemUnequipped.Broadcast(ItemToUnequip);
	OnItemEquipped.Broadcast(ItemToEquip);
}

void UInv_InventoryComponent::Server_RemoveItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	UInv_InventoryItem* Container = Item->ParentContainerItem.Get();
	FInv_InventoryFastArray& List = Container ? Container->InventoryList : InventoryList;
	RemoveRepSubObj(Item);
	List.RemoveEntry(Item);
	if (Container)
	{
		Container->OnItemRemoved.Broadcast(Item);
	}
	else if (Item->GetGridIndex() != INDEX_NONE)
	{
		OnItemRemoved.Broadcast(Item);
	}
	else
	{
		OnItemUnequipped.Broadcast(Item);
	}
}

void UInv_InventoryComponent::ToggleInventoryMenu()
{
	if (!IsValid(InventoryMenu))
		return;
	if (bInventoryMenuOpen)
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
}

void UInv_InventoryComponent::AddRepSubObj(UObject* SubObj)
{
	if (!IsValid(SubObj))
		return;
	if (IsReadyForReplication())
	{
		AddReplicatedSubObject(SubObj);
		if (const UInv_InventoryItem* Item = Cast<UInv_InventoryItem>(SubObj))
			for (const FInv_InventoryEntry& Entry : Item->InventoryList.GetAllItems())
				AddRepSubObj(Entry.GetItem());
	}
}

void UInv_InventoryComponent::RemoveRepSubObj(UObject* SubObj)
{
	if (IsReadyForReplication() && IsValid(SubObj))
	{
		RemoveReplicatedSubObject(SubObj);
		if (const UInv_InventoryItem* Item = Cast<UInv_InventoryItem>(SubObj))
			for (const FInv_InventoryEntry& Entry : Item->InventoryList.GetAllItems())
				RemoveRepSubObj(Entry.GetItem());
	}
}

void UInv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
}

void UInv_InventoryComponent::ConstructInventory()
{
	OwningController = Cast<APlayerController>(GetOwner());
	if (Cast<UInv_FindItemComponent>(this) || !OwningController.IsValid() || !OwningController->IsLocalController())
		return;
	InventoryMenu = CreateWidget<UInv_InventoryBase>(OwningController.Get(), InventoryMenuClass);
	InventoryMenu->AddToViewport();
	InventoryMenu->SetInventoryComponent(this);
	CloseInventoryMenu();
}

void UInv_InventoryComponent::OpenInventoryMenu()
{
	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bInventoryMenuOpen = true;
	// 实际打开客户端界面时隐藏 HUD，避免打开请求等待期间被重新显示。
	if (const AInv_PlayerController* PC = Cast<AInv_PlayerController>(OwningController.Get()); PC && IsValid(PC->HUDWidget))
		PC->HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryMenu->TakeWidget());
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);
}

void UInv_InventoryComponent::OtherOpenInventoryMenu(UInv_InventoryComponent* Inventory)
{
	if (!IsValid(InventoryMenu) || !IsValid(Inventory))
		return;
	OpenInventoryMenu();
	InventoryMenu->SetOpenInventory(Inventory);
}

void UInv_InventoryComponent::CloseInventoryMenu()
{
	InventoryMenu->SetOpenInventory(nullptr);
	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	bInventoryMenuOpen = false;
	if (const AInv_PlayerController* PC = Cast<AInv_PlayerController>(OwningController.Get()); PC && IsValid(PC->HUDWidget))
		PC->HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	FInputModeGameOnly InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(false);
}

