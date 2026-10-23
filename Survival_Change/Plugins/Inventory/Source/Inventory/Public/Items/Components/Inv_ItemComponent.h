#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Inv_ItemComponent.generated.h"

class UInv_InventoryGridAsset;
class UInv_ItemDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDataChanged, UInv_ItemComponent*, ItemComponent);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_ItemComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UInv_ItemComponent();

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 物品资产：在拾取 Actor 的该组件细节面板中直接选择 DA_Item_* 资产即可，
	 * BeginPlay（服务器）会自动把资产烘焙成下面的 ItemManifest，无需手工配置片段。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TObjectPtr<UInv_ItemDefinition> ItemDefinition = nullptr;

	/** 设置物品资产并立即烘焙清单（掉落生成、运行时换物品时使用） */
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void InitializeFromDefinition(UInv_ItemDefinition* InItemDefinition);

	void InitItemManifest(FInv_ItemManifest Manifest);

	FString GetPickUpMessage() const { return PickUpMessage; }

	TArray<FText> GetMessages() const { return Messages; }
	
	FInv_ItemManifest GetItemManifest() const { return ItemManifest; }
	FInv_ItemManifest& GetItemManifestMutable() { return ItemManifest; }

	void PickedUp();
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UInv_InventoryGridAsset> InventoryGridAsset;
	
	FOnDataChanged OnDataChanged;
protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void OnPickedUp();

private:
	/** 若已指定资产，则用资产数据覆盖 ItemManifest */
	void BakeManifestFromDefinition();

	UFUNCTION()
	void OnDateChange();
	
	//选择了资产后隐藏旧的手工清单配置，避免两处数据冲突
	UPROPERTY(ReplicatedUsing=OnDateChange, EditAnywhere, Category = "Inventory",
		meta = (EditCondition = "ItemDefinition == nullptr", EditConditionHides))
	FInv_ItemManifest ItemManifest;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	FString PickUpMessage;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TArray<FText> Messages;
};
