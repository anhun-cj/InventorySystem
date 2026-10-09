#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Manifest/Inv_ItemManifest.h"
#include "Inv_InventoryItem.generated.h"

class UInv_InventoryComponent;

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
	int32 GetOldGridIndex() const {return OldGridIndex;}
	void SetOldGridIndex(int32 Index) {OldGridIndex = Index;}
	void SetGridIndex(int32 Index) {OldGridIndex = GridIndex;GridIndex = Index;}
	void SetItemIndex(int32 Index) {ItemIndex = Index;}
	int32 GetItemIndex() const {return ItemIndex;}
	UInv_InventoryComponent* GetInventoryComponent() const;
	void SetInventoryComponent(UInv_InventoryComponent* InventoryComponent);
	UInv_InventoryComponent* GetOldInventoryComponent() const;
private:
	/*UPROPERTY(VisibleAnywhere, Replicated, meta = (BaseStruct = "/Script/Inventory.FInv_ItemManifest"))
	FInstancedStruct<FInv_ItemManifest> ItemManifest;*/

	//背包物品中的位置
	UPROPERTY(Replicated)
	int32 ItemIndex = 0;
	//有效网格
	UPROPERTY(Replicated)
	int32 GridIndex = 0;
	//旧的有效网格
	UPROPERTY(Replicated)
	int32 OldGridIndex = 0;
	
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryComponent> OldOwningInventoryComponent;

	//引用所属背包组件
	UPROPERTY(Replicated)
	TWeakObjectPtr<UInv_InventoryComponent> OwnerInventory;

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
