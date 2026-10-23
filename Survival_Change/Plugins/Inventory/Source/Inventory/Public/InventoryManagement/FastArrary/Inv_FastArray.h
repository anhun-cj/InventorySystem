#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Inv_FastArray.generated.h"

struct FInv_ItemManifest;
struct FGameplayTag;
class UInv_InventoryItem;
class UInv_InventoryComponent;
class UActorComponent;
class UInv_ItemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryChange, UInv_InventoryItem*, Item);

/// <summary>
/// 这是库存的一个单一条目
/// </summary>
USTRUCT(BlueprintType)
struct FInv_InventoryEntry: public FFastArraySerializerItem
{
	GENERATED_BODY()

	FInv_InventoryEntry() {}
	
	UInv_InventoryItem* GetItem() const { return Item; }

	UPROPERTY()
	int32 GridIndex{INDEX_NONE};

	UPROPERTY()
	int32 SubGridIndex{0};

	UPROPERTY()
	int32 StackCount{1};

private:
	friend struct FInv_InventoryFastArray;
	friend UInv_InventoryComponent;

	UPROPERTY()
	TObjectPtr<UInv_InventoryItem> Item = nullptr;
};

/// <summary>
/// 这是库存的列表
/// </summary>
USTRUCT(BlueprintType)
struct FInv_InventoryFastArray : public FFastArraySerializer
{
	GENERATED_BODY()
	FInv_InventoryFastArray() : Owner(nullptr) {}
	FInv_InventoryFastArray(UObject* InOwner) : Owner(InOwner) {}
	
	TArray<FInv_InventoryEntry> GetAllItems() const;

	//FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemoveIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangeIndices, int32 FinalSize);
	
	//End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParams);

	UInv_InventoryItem* AddEntry(UInv_ItemComponent* ItemComponent, int32 Count, int32 GridIndex, int32 SubGridIndex = 0);
	UInv_InventoryItem* AddEntryOnSplit(UInv_InventoryItem* Item, int32 Index, int32 Count, int32 SubGridIndex = 0);
	UInv_InventoryItem* AddEntryOnEquip(UInv_InventoryItem* Item);
	
	UInv_InventoryItem* AddEntry(UInv_InventoryItem* Item, int32 GridIndex, int32 SubGridIndex = 0);
	void ChangeEntryIndex(const UInv_InventoryItem* Item);
	void ChangeEntry(const UInv_InventoryItem* Item, int32 Count);
	void ChangeEntryByIndex(const UInv_InventoryItem* Item, int32 Count);
	
	void ChangeEntryOnEquip(UInv_InventoryItem* Item);
	
	void RemoveEntry(UInv_InventoryItem* Item);
	void ClearAllItems();

	UInv_InventoryItem* FindNoFillItemLike(const FInv_ItemManifest& Source, TSet<UInv_InventoryItem*> Items) const;
	UInv_InventoryItem* FindFirstNoFillItemLike(const FInv_ItemManifest& Source) const;
	UInv_InventoryItem* FindItem(const UInv_InventoryItem* Item) const;
	bool CheckItemFilled(UInv_InventoryItem* Item) const;
	
private:
	friend UInv_InventoryComponent;
	friend class UInv_FindItemComponent;
	UInv_InventoryComponent* GetInventoryComponent() const;
	//物品同步列表
	UPROPERTY()
	TArray<FInv_InventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UObject> Owner;

};

template<>
struct TStructOpsTypeTraits<FInv_InventoryFastArray> : public TStructOpsTypeTraitsBase2<FInv_InventoryFastArray>
{
	enum { WithNetDeltaSerializer = true };
};
