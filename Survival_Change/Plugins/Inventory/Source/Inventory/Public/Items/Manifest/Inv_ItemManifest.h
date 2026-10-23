#pragma once

#include "CoreMinimal.h"
#include "Types/Inv_GridTypes.h"
#include "StructUtils/InstancedStruct.h"
#include "GameplayTagContainer.h"
#include "Inv_ItemManifest.generated.h"

/*
 * 这个清单包含所有物品的数据
 */

class UInv_ItemComponent;
class UInv_CompositeBase;
class UInv_ItemDefinition;
struct FInv_ItemFragment;

USTRUCT(BlueprintType)
struct INVENTORY_API FInv_ItemManifest
{
	GENERATED_BODY()
public:
	
	TArray<TInstancedStruct<FInv_ItemFragment>>& GetAllFragments() {return Fragments;};
	
	UInv_InventoryItem* Manifest(UObject* NewOuter);
	UInv_InventoryItem* NewManifest(UObject* NewOuter) const;
	
	EInv_ItemCategory GetItemCategory() const { return ItemCategory;}
	void SetItemCategory(EInv_ItemCategory InCategory) { ItemCategory = InCategory; }

	FGameplayTag GetItemType() const { return ItemType; }
	void SetItemType(FGameplayTag InType) { ItemType = InType; }

	/** 物品唯一ID（来源于 UInv_ItemDefinition::ItemId），用于判断两个物品实例是否为同一种物品 */
	FName GetItemId() const { return ItemId; }
	void SetItemId(FName InItemId) { ItemId = InItemId; }

	/** 获取来源物品资产软引用（可能未加载） */
	const TSoftObjectPtr<UInv_ItemDefinition>& GetSourceDefinition() const { return SourceDefinition; }
	void SetSourceDefinition(const TSoftObjectPtr<UInv_ItemDefinition>& InDefinition) { SourceDefinition = InDefinition; }

	/**
	 * 判断两个物品清单是否为同一种物品。
	 * 优先比较唯一 ItemId；若任一方没有 ItemId（例如非资产方式手工配置的旧物品），
	 * 回退为按物品类型标签精确匹配，保证旧内容兼容。
	 */
	static bool AreSameItem(const FInv_ItemManifest& A, const FInv_ItemManifest& B)
	{
		if (!A.ItemId.IsNone() && !B.ItemId.IsNone())
		{
			return A.ItemId == B.ItemId;
		}
		return A.ItemType.IsValid() && A.ItemType.MatchesTagExact(B.ItemType);
	}

	void SetPickupActorClass(TSubclassOf<AActor> InClass) { PickupActorClass = InClass; }
	
	void AssimilateInventoryFragment(UInv_CompositeBase* Composite) const;
	
	template<typename T>
	requires std::derived_from<T, FInv_ItemFragment>
	const T* GetFragmentOfTypeWithTag(const FGameplayTag& FragmentTag) const;
	
	template<typename T>
	requires std::derived_from<T, FInv_ItemFragment>
	const T* GetFragmentOfType() const;
	
	template<typename T>
	requires std::derived_from<T, FInv_ItemFragment>
	T* GetFragmentOfTypeMutable();
	
	template<typename T>
	requires std::derived_from<T, FInv_ItemFragment>
	TArray<const T*> GetAllFragmentsOfType() const;
	
	UInv_ItemComponent* SpawnPickUpActor(const UObject* WorldContextObject, const FVector& SpawnLocation, const FRotator& SpawnRotation) const;
private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FInv_ItemFragment>> Fragments;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	EInv_ItemCategory ItemCategory = EInv_ItemCategory::None;
	
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "GameItems"))
	FGameplayTag ItemType;

	/** 物品唯一ID；由物品资产(UInv_ItemDefinition)生成物品时写入，相同物品判断以它为准 */
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FName ItemId;

	/** 生成该清单的物品资产软引用；掉落重生时据此把资产重新回传给拾取组件 */
	UPROPERTY()
	TSoftObjectPtr<UInv_ItemDefinition> SourceDefinition;

	UPROPERTY()
	TSubclassOf<AActor> PickupActorClass;
	
	void ClearFragments();
};

template<typename T>
requires std::derived_from<T, FInv_ItemFragment>
const T* FInv_ItemManifest::GetFragmentOfTypeWithTag(const FGameplayTag& FragmentTag) const
{
	for (const TInstancedStruct<FInv_ItemFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			if (!FragmentPtr->GetFragmentTag().MatchesTagExact(FragmentTag))
				continue;
			return FragmentPtr;
		}
	}
	return nullptr;
}

template <typename T> requires std::derived_from<T, FInv_ItemFragment>
const T* FInv_ItemManifest::GetFragmentOfType() const
{
	for (const TInstancedStruct<FInv_ItemFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			return FragmentPtr;
		}
	}
	return nullptr;
}

template <typename T> requires std::derived_from<T, FInv_ItemFragment>
T* FInv_ItemManifest::GetFragmentOfTypeMutable()
{
	for (TInstancedStruct<FInv_ItemFragment>& Fragment : Fragments)
	{
		if (T* FragmentPtr = Fragment.GetMutablePtr<T>())
		{
			return FragmentPtr;
		}
	}
	return nullptr;
}

template <typename T> requires std::derived_from<T, FInv_ItemFragment>
TArray<const T*> FInv_ItemManifest::GetAllFragmentsOfType() const
{
	TArray<const T*> Result;
	for (const TInstancedStruct<FInv_ItemFragment>& Fragment : Fragments)
	{
		if (const T* FragmentPtr = Fragment.GetPtr<T>())
		{
			Result.Add(FragmentPtr);
		}
	}
	return Result;
}
