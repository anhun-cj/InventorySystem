#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Types/Inv_GridTypes.h"
#include "Inv_ItemStats.h"
#include "Inv_ItemDefinition.generated.h"

class AActor;
class UTexture2D;
class UInv_InventoryItem;
class AInv_EquipActor;
struct FInv_ItemManifest;

/**
 * 物品静态定义资产。物品只需引用这个资产即可获得全部信息：
 * 共用名称、ItemId、图标、标签与占格；普通物品配置获取途径，效果与制作材料由对应子类配置。
 * 背包、制作、分解、掉落和 UI 都应该引用这个资产，避免在多处重复维护道具字段。
 * 全部资产可由物品 DataTable（FInv_ItemDefinitionTableRow）批量导入生成。
 */
UCLASS(BlueprintType)
class INVENTORY_API UInv_ItemDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	static const FPrimaryAssetType PrimaryAssetType;

	/** 独立配置的稳定数字道具编号，例如 10001；不得使用道具名称或资产名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FName ItemId = NAME_None;

	/** 背包、拾取提示中显示的名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText DisplayName;

	/** 物品详细描述，支持多行文本。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	/** 背包和拾取提示中使用的图标。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 单个堆叠的数量上限；1 表示不可堆叠。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stacking", meta = (ClampMin = 1))
	int32 MaxStackSize = 1;

	/** 生成物品时的默认堆叠数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stacking", meta = (ClampMin = 1))
	int32 DefaultStackCount = 1;

	/** 静态物品标签，用于分类，例如 GameItems.Equipment.Weapons.Axe。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Tags", meta = (Categories = "GameItems"))
	FGameplayTagContainer ItemTags;

	/** 物品在背包网格中占据的格子尺寸。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Grid")
	FIntPoint GridSize{ 1, 1 };

	/** 物品图标在网格中的内边距。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Grid")
	float GridPadding = 0.0f;

	/** 普通物品的获取途径，仅用于详情展示，不负责实际发放物品。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Acquisition", meta = (MultiLine = "true", DisplayName = "获取途径"))
	FText AcquisitionSources;

	/** 分解该物品后生成的物品与数量；为空表示该物品不可分解。 */
	UPROPERTY(BlueprintReadOnly, Category = "Item|Decompose")
	TArray<FInv_ItemAmount> DecomposeResults;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	/** 返回独立配置的 ItemId；未配置时返回 NAME_None。 */
	UFUNCTION(BlueprintPure, Category = "Item")
	FName GetResolvedItemId() const { return ItemId; }

	/** 是否可堆叠（MaxStackSize > 1）。 */
	UFUNCTION(BlueprintPure, Category = "Item")
	bool IsStackable() const;

	/** 是否可制作（配置了制作材料）。 */
	UFUNCTION(BlueprintPure, Category = "Item|Crafting")
	bool IsCraftable() const;

	/** 是否可分解（配置了分解产物）。 */
	UFUNCTION(BlueprintPure, Category = "Item|Decompose")
	bool IsDecomposable() const { return DecomposeResults.Num() > 0; }

	/** 由资产类型确定物品分类；可制作是装备、消耗品的独立配置。 */
	UFUNCTION(BlueprintPure, Category = "Item|Type")
	EInv_ItemCategory GetItemCategory() const;

	/** 判断物品是否属于指定类型；支持传入 GameItems.Equipment 等父标签进行层级匹配。 */
	UFUNCTION(BlueprintPure, Category = "Item|Type")
	bool HasItemType(FGameplayTag ItemTypeTag) const;

	/**
	 * 把资产字段组装成 FInv_ItemManifest
	 * （Grid/Image/Text/Stackable/LabeledNumber/Equipment/Consume 片段），不创建运行时物品对象。
	 * 供 UInv_ItemComponent 烘焙清单使用。
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	void BuildItemManifest(FInv_ItemManifest& OutManifest) const;

	/**
	 * 用该定义生成一个运行时背包物品：组装 FInv_ItemManifest 后走现有 Manifest 流程。
	 */
	UFUNCTION(BlueprintCallable, Category = "Item")
	UInv_InventoryItem* ManifestItem(UObject* Outer) const;
};

/** 可放入装备槽并在装备期间生效的装备定义。 */
UCLASS(BlueprintType, HideCategories = ("Item|Stacking", "Item|Acquisition"))
class INVENTORY_API UInv_EquipmentItemDefinition : public UInv_ItemDefinition
{
	GENERATED_BODY()

public:
	/** 同一份效果用于装备生效及详情显示。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (DisplayName = "装备效果"))
	TArray<FInv_ItemStatModifier> BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (DisplayName = "制作所需材料"))
	TArray<FInv_ItemAmount> CraftingIngredients;

	/** 允许放入的装备槽标签，例如 Equipment.Slot.Weapon。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FGameplayTag EquipmentSlotTag;

	/** 装备时生成并挂到角色身上的 Actor 类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	TSubclassOf<AInv_EquipActor> EquipActorClass = nullptr;

	/** 装备 Actor 挂接的骨骼插槽名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment")
	FName SocketAttachPoint{ NAME_None };
};

/** 使用后触发数值效果的消耗品定义。 */
UCLASS(BlueprintType, HideCategories = ("Item|Acquisition"))
class INVENTORY_API UInv_ConsumableItemDefinition : public UInv_ItemDefinition
{
	GENERATED_BODY()

public:
	/** 使用时生效的数值效果列表（如恢复生命/法力），对应 ConsumeFragment 的修改器数值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (DisplayName = "消耗效果"))
	TArray<FInv_ItemStatModifier> ConsumeEffects;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (DisplayName = "制作所需材料"))
	TArray<FInv_ItemAmount> CraftingIngredients;
};
