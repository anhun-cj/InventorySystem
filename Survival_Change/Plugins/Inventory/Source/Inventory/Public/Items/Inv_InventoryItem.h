#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Manifest/Inv_ItemManifest.h"
#include "InventoryManagement/FastArrary/Inv_FastArray.h"
#include "Inv_InventoryItem.generated.h"

class UInv_InventoryComponent;
class UInv_InventoryGridAsset;

UCLASS()
class INVENTORY_API UInv_InventoryItem : public UObject
{
	GENERATED_BODY()
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool IsSupportedForNetworking() const override { return true; };
	
	void SetItemManifest(const FInv_ItemManifest& Manifest);
	
	const FInv_ItemManifest& GetItemManifest() const { return ItemManifest.Get<FInv_ItemManifest>(); }
	FInv_ItemManifest& GetItemManifestMutable() { return ItemManifest.GetMutable<FInv_ItemManifest>(); }
	bool IsStackable() const;
	bool IsConsumable() const;
	int32 GetGridIndex() const {return GridIndex;}
	int32 GetSubGridIndex() const {return SubGridIndex;}
	void SetSubGridIndex(int32 Index) {SubGridIndex = Index;}
	int32 GetOldGridIndex() const {return OldGridIndex;}
	void SetGridIndex(int32 Index) {OldGridIndex = GridIndex;GridIndex = Index;}
	void SetItemIndex(int32 Index) {ItemIndex = Index;}
	int32 GetItemIndex() const {return ItemIndex;}
	UInv_InventoryComponent* GetInventoryComponent() const;
	void SetInventoryComponent(UInv_InventoryComponent* InventoryComponent);
	UInv_InventoryComponent* GetOldInventoryComponent() const{return OldOwnerInventory.Get();}
	
	// 直接收纳这个物品的容器；为空表示位于组件顶层库存或装备槽。
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryItem> ParentContainerItem;
	
	// 物品自身的内部库存；容器进入其他库存后，内容仍随这个物品保存和复制。
	UPROPERTY(Replicated)
	FInv_InventoryFastArray InventoryList{this};
	
	UPROPERTY(Replicated)
	TObjectPtr<UInv_InventoryGridAsset> GridAsset;

	// 附近列表中的容器条目指向地面真实库存，供客户端查询内容和预览存入空间。
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryComponent> GroundInventory;
	
	FInventoryChange OnItemAdded;
	FInventoryChange OnItemChanged;
	FInventoryChange OnItemRemoved;
private:
	friend class UInv_SpatialInventory;
	/*UPROPERTY(VisibleAnywhere, Replicated, meta = (BaseStruct = "/Script/Inventory.FInv_ItemManifest"))
	FInstancedStruct<FInv_ItemManifest> ItemManifest;*/

	//背包物品中的位置
	UPROPERTY(Replicated)
	int32 ItemIndex = 0;
	//有效网格
	UPROPERTY(Replicated)
	int32 GridIndex = 0;

	// 同一库存内的子网格编号，GridIndex 是该子网格内的局部格位。
	UPROPERTY(Replicated)
	int32 SubGridIndex = 0;

	UPROPERTY(Replicated)
	int32 OldGridIndex = 0;
	
	//负责管理和复制该物品的库存组件，直接收纳关系由 ParentContainerItem 表示。
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryComponent> OwnerInventory;
	
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryComponent> OldOwnerInventory;

	UPROPERTY(VisibleAnywhere, Replicated)
	TInstancedStruct<FInv_ItemManifest> ItemManifest;
	
};

template<typename FragmentType>
const FragmentType* GetFragment(const UInv_InventoryItem* Item, const FGameplayTag& Tag)
{
	if (!IsValid(Item))
		return nullptr;
	const FInv_ItemManifest& Manifest = Item->GetItemManifest(); // 用引用，不要拷贝

	return Manifest.GetFragmentOfTypeWithTag<FragmentType>(Tag);
}
