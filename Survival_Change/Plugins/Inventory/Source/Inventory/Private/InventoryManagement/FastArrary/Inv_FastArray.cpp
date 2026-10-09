#include "InventoryManagement/FastArrary/Inv_FastArray.h"
#include "Items/Inv_InventoryItem.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Manifest/Inv_ItemManifest.h"

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

void FInv_InventoryFastArray::PreReplicatedRemove(const TArrayView<int32> RemoveIndices, int32 FinalSize)
{
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;
	for (int32 Index : RemoveIndices)
	{
		IC->OnItemRemoved.Broadcast(Entries[Index].Item);
	}
}

void FInv_InventoryFastArray::PostReplicatedAdd(const TArrayView<int32> AddIndices, int32 FinalSize)
{
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;
	for (int32 Index : AddIndices)
	{
		IC->OnItemAdded.Broadcast(Entries[Index].Item);
	}
}

void FInv_InventoryFastArray::PostReplicatedChange(const TArrayView<int32> ChangeIndices, int32 FinalSize)
{
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;
	for (int32 Index : ChangeIndices)
	{
		IC->OnItemChanged.Broadcast(Entries[Index].Item);
	}
}

UInv_InventoryItem* FInv_InventoryFastArray::AddEntry(UInv_ItemComponent* ItemComponent, int32 Count, int32 GridIndex)
{
	if (!IsValid(OwnerComponent))
		return nullptr;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;
	
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return nullptr;
	
	
	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = ItemComponent->GetItemManifest().Manifest(OwningActor);
	IC->AddRepSubObj(NewEntry.Item);

	if (FInv_StackableFragment* StackableFragment = NewEntry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Count);
	}
	NewEntry.Item->SetGridIndex(GridIndex);
	NewEntry.Item->SetOldGridIndex(GridIndex);
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);
	
	MarkItemDirty(NewEntry);
	return NewEntry.Item;
}

UInv_InventoryItem* FInv_InventoryFastArray::AddEntry(UInv_InventoryItem* Item, int32 GridIndex)
{
	if (!IsValid(OwnerComponent))
		return nullptr;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return nullptr;

	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return nullptr;

	FInv_InventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Item = Item;
	// 跨背包转移时物品会换属主组件，必须重新注册为目标组件的复制子对象，
	// 否则客户端收不到物品的 fragment/堆叠数据
	IC->AddRepSubObj(Item);
	NewEntry.Item->SetGridIndex(GridIndex);
	NewEntry.Item->SetOldGridIndex(GridIndex);
	NewEntry.Item->SetItemIndex(Entries.Num() - 1);

	MarkItemDirty(NewEntry);

	return Item;
}

void FInv_InventoryFastArray::ChangeEntryIndex(const UInv_InventoryItem* Item, const int32 Index)
{
	if (!IsValid(OwnerComponent))
		return;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return;
	
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;

	if (!Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[Item->GetItemIndex()];

	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::ChangeEntry(const UInv_InventoryItem* Item, const int32 Count)
{
	if (!IsValid(OwnerComponent))
		return;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return;
	
	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;
	
	UInv_InventoryItem* OldItem = FindFirstNoFillItemLike(Item->GetItemManifest());
	if (!IsValid(OldItem))
	{
		//AddEntry(Item);
		return;
	}
	if (!Entries.IsValidIndex(OldItem->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[OldItem->GetItemIndex()];
	FInv_StackableFragment* StackableFragment = Entry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>();
	if (!StackableFragment)
		return;
	StackableFragment->SetStackCount(StackableFragment->GetStackCount() + Count);
	
	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::ChangeEntryByIndex(const UInv_InventoryItem* Item, const int32 Count)
{
	if (!IsValid(OwnerComponent))
		return;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;

	//直接用 ItemIndex 定位条目，避免按 OldGridIndex 匹配到错误的物品
	if (!Entries.IsValidIndex(Item->GetItemIndex()))
		return;
	FInv_InventoryEntry& Entry = Entries[Item->GetItemIndex()];
	FInv_StackableFragment* StackableFragment = Entry.Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>();
	if (!StackableFragment)
		return;
	StackableFragment->SetStackCount(Count);

	MarkItemDirty(Entry);
}

void FInv_InventoryFastArray::RemoveEntry(UInv_InventoryItem* Item)
{
	if (!IsValid(OwnerComponent))
		return;
	AActor* OwningActor = OwnerComponent->GetOwner();
	if (!OwningActor->HasAuthority())
		return;

	UInv_InventoryComponent* IC = Cast<UInv_InventoryComponent>(OwnerComponent);
	if (!IsValid(IC))
		return;
	
	if (!IsValid(Item) || !Entries.IsValidIndex(Item->GetItemIndex()))
		return;
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

UInv_InventoryItem* FInv_InventoryFastArray::FindItemByIndex(int32 Index) const
{
	const FInv_InventoryEntry* FoundItem = Entries.FindByPredicate([this, Index](const FInv_InventoryEntry& Entry)
	{
		return IsValid(Entry.Item) && Entry.Item->GetOldGridIndex() == Index;
	});
	return  FoundItem ? FoundItem->Item : nullptr;
}

bool FInv_InventoryFastArray::CheckItemFilled(UInv_InventoryItem* Item) const
{
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	return StackableFragment ? StackableFragment->GetStackCount() == StackableFragment->GetMaxStackCount() : true;
}
