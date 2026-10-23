#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inv_ItemStats.h"
#include "Inv_ItemDefinitionTableRow.generated.h"

class UInv_ItemDefinition;

/** 仅用于选择独立导入表的类型，不再作为表格中的配置字段。 */
UENUM(BlueprintType)
enum class EInv_ItemDefinitionKind : uint8
{
	Base UMETA(DisplayName = "普通物品"),
	Equipment UMETA(DisplayName = "装备"),
	Consumable UMETA(DisplayName = "消耗品")
};

/** 三类独立表共用的基础字段；CSV 行名为稳定数字 ItemId。 */
USTRUCT(BlueprintType, meta = (Hidden))
struct INVENTORY_API FInv_ItemDefinitionTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resolved Item", meta = (DataTableImportOptional))
	TObjectPtr<UInv_ItemDefinition> AssetPath = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FText ItemName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	FIntPoint GridSize{1, 1};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	float GridPadding = 0.0f;

	/** 多个数字 TagId 使用 | 分隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FString ItemTagIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resolved Item", meta = (DataTableImportOptional))
	FGameplayTagContainer ItemTags;

	/** 保留现有分解数据及导入逻辑，本次不扩展分解配置。 */
	UPROPERTY(meta = (HideFromDataTableEditorColumn, DataTableImportOptional))
	FString DecomposeResultIds;

	UPROPERTY(meta = (DataTableImportOptional))
	TArray<FInv_ItemAmount> DecomposeResults;
};

/** 普通物品：不能使用、装备或制作，只配置获取途径。 */
USTRUCT(BlueprintType)
struct INVENTORY_API FInv_CommonItemDefinitionTableRow : public FInv_ItemDefinitionTableRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStackSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 DefaultStackCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (MultiLine = "true", DisplayName = "获取途径"))
	FText AcquisitionSources;
};

/** 装备：不提供堆叠或消耗效果字段，外观资源在装备资产中配置。 */
USTRUCT(BlueprintType)
struct INVENTORY_API FInv_EquipmentItemDefinitionTableRow : public FInv_ItemDefinitionTableRow
{
	GENERATED_BODY()

	/** TagId&Value|TagId&Value。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FString BaseStats;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (DataTableImportOptional, DisplayName = "装备效果"))
	TArray<FInv_ItemStatModifier> ItemStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FName EquipmentSlotTagId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment", meta = (DataTableImportOptional))
	FGameplayTag EquipmentSlot;

	/** ItemId&Count|ItemId&Count；为空表示不能制作。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FString CraftingIngredientIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (DataTableImportOptional, DisplayName = "制作所需材料"))
	TArray<FInv_ItemAmount> CraftingIngredients;
};

/** 消耗品：仅有消耗效果、可选制作材料及堆叠配置。 */
USTRUCT(BlueprintType)
struct INVENTORY_API FInv_ConsumableItemDefinitionTableRow : public FInv_ItemDefinitionTableRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 MaxStackSize = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 DefaultStackCount = 1;

	/** TagId&Value|TagId&Value。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FString ConsumeEffects;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Consumable", meta = (DataTableImportOptional, DisplayName = "消耗效果"))
	TArray<FInv_ItemStatModifier> ConsumeEffectList;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Import")
	FString CraftingIngredientIds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crafting", meta = (DataTableImportOptional, DisplayName = "制作所需材料"))
	TArray<FInv_ItemAmount> CraftingIngredients;
};
