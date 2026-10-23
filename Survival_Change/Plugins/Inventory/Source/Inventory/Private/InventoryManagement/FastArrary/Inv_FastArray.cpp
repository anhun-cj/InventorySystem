#include "InventoryManagement/FastArrary/Inv_FastArray.h"
#include "Items/Inv_InventoryItem.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Manifest/Inv_ItemManifest.h"

// 使用引擎 FastArray 增量复制条目，同时记录当前数组属于组件还是容器物品。
bool FInv_InventoryFastArray::NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams)
{
	Owner = DeltaParams.Object;
	return FastArrayDeltaSerialize<FInv_InventoryEntry, FInv_InventoryFastArray>(Entries, DeltaParams, *this);
}

// 顶层库存直接取所属组件；物品内部库存通过容器物品取得管理组件。
UInv_InventoryComponent* FInv_InventoryFastArray::GetInventoryComponent() const
{
	if (const UInv_InventoryItem* Item = Cast<UInv_InventoryItem>(Owner))
		return Item->GetInventoryComponent();
	return Cast<UInv_InventoryComponent>(Owner);
}

TArray<FInv_InventoryEntry> FInv_InventoryFastArray::GetAllItems()const
{
	TArray<FInv_InventoryEntry> Results;
	Results.Reserve(Entries.Num());

	for (const FInv_InventoryEntry& Entry : Entries)
	{
		if (!IsValid(Entry.Item))
			continue;
		Results.Add(Entry);
	}
	return Results;
}

// 客户端删除复制条目前通知对应 UI；装备槽条目和普通网格条目分别广播。
void FInv_InventoryFastArray::PreReplicatedRemove(const TArrayView<int32> RemoveIndices, int32 FinalSize)
{
	if (UInv_InventoryItem* ContainerItem = Cast<UInv_InventoryItem>(Owner))
	{
		for (int32 Index : RemoveIndices)
			ContainerItem->OnItemRemoved.Broadcast(Entries[Index].Item);
		return;
	}
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	for (int32 Index : RemoveIndices)
	{
		if (Entries[Index].GridIndex != INDEX_NONE)
		{
			IC->OnItemRemoved.Broadcast(Entries[Index].Item);
		}
		else
		{
			IC->OnItemUnequipped.Broadcast(Entries[Index].Item);
		}
	}
}

// 客户端收到新增条目后，通知容器网格、普通网格或装备槽生成显示。
void FInv_InventoryFastArray::PostReplicatedAdd(const TArrayView<int32> AddIndices, int32 FinalSize)
{
	if (UInv_InventoryItem* ContainerItem = Cast<UInv_InventoryItem>(Owner))
	{
		for (int32 Index : AddIndices)
			ContainerItem->OnItemAdded.Broadcast(Entries[Index].Item);
		return;
	}
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	for (int32 Index : AddIndices)
	{
		if (Entries[Index].GridIndex != INDEX_NONE)
		{
			IC->OnItemAdded.Broadcast(Entries[Index].Item);
		}
		else
		{
			IC->OnItemEquipped.Broadcast(Entries[Index].Item);
		}
	}
}

// 数量、格位等复制数据改变后，通知对应库存刷新物品显示。
void FInv_InventoryFastArray::PostReplicatedChange(const TArrayView<int32> ChangeIndices, int32 FinalSize)
{
	if (UInv_InventoryItem* ContainerItem = Cast<UInv_InventoryItem>(Owner))
	{
		for (int32 Index : ChangeIndices)
			ContainerItem->OnItemChanged.Broadcast(Entries[Index].Item);
		return;
	}
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	for (int32 Index : ChangeIndices)
	{
		if (Entries[Index].GridIndex != INDEX_NONE)
		{
			IC->OnItemChanged.Broadcast(Entries[Index].Item);
		}
		else
		{
			IC->OnItemEquipped.Broadcast(Entries[Index].Item);
		}
	}
}

// 从地面物品创建库存条目，并处理容器内容从地面到库存的交接。
UInv_InventoryItem* FInv_InventoryFastArray::AddEntry(UInv_ItemComponent* ItemComponent, int32 Count, int32 GridIndex, int32 SubGridIndex)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return nullptr;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;
	
	
	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	FInv_ItemManifest ItemManifest = ItemComponent->GetItemManifest();
	ItemManifest.SetPickupActorClass(ItemComponent->GetOwner()->GetClass());
	NewEntry.Item = ItemManifest.Manifest(OwningActor);
	NewEntry.Item->GridAsset = ItemComponent->InventoryGridAsset;
	NewEntry.Item->ParentContainerItem = Cast<UInv_InventoryItem>(Owner);
	NewEntry.Item->SetInventoryComponent(IC);
	if (NewEntry.Item->GridAsset)
	{
		UInv_InventoryComponent* GroundInventory = ItemComponent->GetOwner()->FindComponentByClass<UInv_InventoryComponent>();
		// 附近列表只登记显示条目，内容继续保存在地面 Actor 的库存中。
		if (Cast<UInv_FindItemComponent>(IC))
			NewEntry.Item->GroundInventory = GroundInventory;
		else
		{
			// 真正拾取时将原有内容交给容器物品，保留格位并更新子物品归属。
			FInv_InventoryFastArray& GroundList = GroundInventory->GetInventoryListMutable();
			const TArray<FInv_InventoryEntry> Contents = GroundList.GetAllItems();
			for (const FInv_InventoryEntry& Entry : Contents)
				GroundInventory->RemoveRepSubObj(Entry.GetItem());
			GroundList.ClearAllItems();
			NewEntry.Item->InventoryList.Entries = Contents;
			for (FInv_InventoryEntry& Entry : NewEntry.Item->InventoryList.Entries)
			{
				Entry.GetItem()->ParentContainerItem = NewEntry.Item;
				Entry.GetItem()->SetInventoryComponent(IC);
				NewEntry.Item->InventoryList.MarkItemDirty(Entry);
				GroundInventory->OnItemRemoved.Broadcast(Entry.GetItem());
			}
		}
	}
	IC->AddRepSubObj(NewEntry.Item);

	if (FInv_StackableFragment* StackableFragment = NewEntry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Count);
	}
	NewEntry.Item->SetGridIndex(GridIndex);
	NewEntry.Item->SetSubGridIndex(SubGridIndex);
	NewEntry.SubGridIndex = SubGridIndex;
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);
	NewEntry.GridIndex = GridIndex;
	NewEntry.StackCount = Count;
	
	MarkItemDirty(NewEntry);
	return NewEntry.Item;
}

UInv_InventoryItem* FInv_InventoryFastArray::AddEntryOnSplit(UInv_InventoryItem* Item, int32 Index, int32 Count, int32 SubGridIndex)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return nullptr;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;
	
	
	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item->GetItemManifestMutable().NewManifest(OwningActor);
	NewEntry.Item->GridAsset = Item->GridAsset;
	NewEntry.Item->ParentContainerItem = Cast<UInv_InventoryItem>(Owner);
	NewEntry.Item->SetInventoryComponent(IC);
	NewEntry.Item->SetGridIndex(Index);
	NewEntry.Item->SetSubGridIndex(SubGridIndex);
	NewEntry.SubGridIndex = SubGridIndex;
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);
	IC->AddRepSubObj(NewEntry.Item);

	if (FInv_StackableFragment* StackableFragment = NewEntry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Count);
	}
	NewEntry.GridIndex = Index;
	NewEntry.StackCount = Count;
	MarkItemDirty(NewEntry);
	return NewEntry.Item;
}

UInv_InventoryItem* FInv_InventoryFastArray::AddEntryOnEquip(UInv_InventoryItem* Item)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return nullptr;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;
	
	
	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;
	Item->ParentContainerItem.Reset();
	IC->AddRepSubObj(NewEntry.Item);

	NewEntry.Item->SetGridIndex(INDEX_NONE);
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);
	MarkItemDirty(NewEntry);
	return NewEntry.Item;
}

UInv_InventoryItem* FInv_InventoryFastArray::AddEntry(UInv_InventoryItem* Item, int32 GridIndex, int32 SubGridIndex)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return nullptr;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;

	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;
	Item->ParentContainerItem = Cast<UInv_InventoryItem>(Owner);
	if (Item->ParentContainerItem.IsValid())
		Item->SetInventoryComponent(IC);
	// 跨背包转移时物品会换属主组件，必须重新注册为目标组件的复制子对象，
	IC->AddRepSubObj(Item);
	NewEntry.Item->SetGridIndex(GridIndex);
	NewEntry.Item->SetSubGridIndex(SubGridIndex);
	NewEntry.SubGridIndex = SubGridIndex;
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);
	NewEntry.GridIndex = GridIndex;
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	NewEntry.StackCount = StackableFragment ? StackableFragment->GetStackCount() : 1;

	MarkItemDirty(NewEntry);

	return Item;
}

void FInv_InventoryFastArray::ChangeEntryIndex(const UInv_InventoryItem* Item)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	if (!Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[Item->GetItemIndex()];
	Entry.GridIndex = Item->GetGridIndex();
	Entry.SubGridIndex = Item->GetSubGridIndex();

	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::ChangeEntry(const UInv_InventoryItem* Item, const int32 Count)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	UInv_InventoryItem* OldItem = FindFirstNoFillItemLike(Item->GetItemManifest());
	if (!IsValid(OldItem))
	{
		return;
	}
	if (!Entries.IsValidIndex(OldItem->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[OldItem->GetItemIndex()];
	FInv_StackableFragment* StackableFragment = Entry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>();
	if (!StackableFragment)
		return;
	StackableFragment->SetStackCount(StackableFragment->GetStackCount() + Count);
	Entry.StackCount = StackableFragment->GetStackCount();
	
	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::ChangeEntryByIndex(const UInv_InventoryItem* Item, const int32 Count)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	//直接用 ItemIndex 定位条目，避免按 OldGridIndex 匹配到错误的物品
	if (!Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[Item->GetItemIndex()];
	FInv_StackableFragment* StackableFragment = Entry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>();
	if (!StackableFragment)
	{
		MarkItemDirty(Entry);
		return;
	}
	StackableFragment->SetStackCount(Count);
	Entry.StackCount = Count;
	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::ChangeEntryOnEquip(UInv_InventoryItem* Item)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	//直接用 ItemIndex 定位条目，避免按 OldGridIndex 匹配到错误的物品
	if (!Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	
	if (UInv_InventoryItem* OldItem = FindItem(Item))
	{
		FInv_InventoryEntry& Entry = Entries[OldItem->GetItemIndex()];
		Entry.Item->ParentContainerItem.Reset();
		Entry.Item->SetGridIndex(INDEX_NONE);
		Entry.GridIndex = INDEX_NONE;
		MarkItemDirty(Entry);
	}
	else
	{
		FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
		NewEntry.Item = Item;
		Item->ParentContainerItem.Reset();
		IC->AddRepSubObj(Item);
		NewEntry.Item->SetItemIndex(Entries.Num() - 1);
		NewEntry.Item->SetGridIndex(INDEX_NONE);
		MarkItemDirty(NewEntry);
	}
}

void FInv_InventoryFastArray::RemoveEntry(UInv_InventoryItem* Item)
{
	UInv_InventoryComponent* IC = GetInventoryComponent();
	if (!IsValid(IC))
		return;
	AActor* OwningActor = IC->GetOwner();
	if (!OwningActor->HasAuthority())
		return;
	
	if (!IsValid(Item) || !Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	
	Item->ParentContainerItem.Reset();
	Entries.RemoveAt(Item->GetItemIndex());
	
	//整体排序可能改变,须重算所有物品的 ItemIndex，
	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		if (IsValid(Entries[i].Item) && Entries[i].Item->GetItemIndex() != i)
			Entries[i].Item->SetItemIndex(i);
	}

	MarkArrayDirty();
}

void FInv_InventoryFastArray::ClearAllItems()
{
	for (const FInv_InventoryEntry& Entry : Entries)
		if (IsValid(Entry.Item))
			Entry.Item->ParentContainerItem.Reset();
	Entries.Empty();
	MarkArrayDirty();
}

UInv_InventoryItem* FInv_InventoryFastArray::FindNoFillItemLike(const FInv_ItemManifest& Source, TSet<UInv_InventoryItem*> Items) const
{
	const FInv_InventoryEntry* FoundItem = Entries.FindByPredicate([this, &Source, &Items](const FInv_InventoryEntry& Entry)
	{
		return IsValid(Entry.Item) && FInv_ItemManifest::AreSameItem(Entry.Item->GetItemManifest(), Source) && !CheckItemFilled(Entry.Item) && !Items.Contains(Entry.Item);
	});
	return  FoundItem ? FoundItem->Item : nullptr;
}

UInv_InventoryItem* FInv_InventoryFastArray::FindFirstNoFillItemLike(const FInv_ItemManifest& Source) const
{
	const FInv_InventoryEntry* FoundItem = Entries.FindByPredicate([this, &Source](const FInv_InventoryEntry& Entry)
	{
		return IsValid(Entry.Item) && FInv_ItemManifest::AreSameItem(Entry.Item->GetItemManifest(), Source) && !CheckItemFilled(Entry.Item);
	});
	return  FoundItem ? FoundItem->Item : nullptr;
}

UInv_InventoryItem* FInv_InventoryFastArray::FindItem(const UInv_InventoryItem* Item) const
{
	const FInv_InventoryEntry* FoundItem = Entries.FindByPredicate([this, Item](const FInv_InventoryEntry& Entry)
	{
		return IsValid(Entry.Item) && Entry.Item == Item;
	});
	return  FoundItem ? FoundItem->Item : nullptr;
}

bool FInv_InventoryFastArray::CheckItemFilled(UInv_InventoryItem* Item) const
{
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	return StackableFragment ? StackableFragment->GetStackCount() == StackableFragment->GetMaxStackCount() : true;
}

