#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inv_ItemStats.generated.h"

class UInv_ItemDefinition;

/** 物品上的数值词条，例如攻击力、生命值等；每个物品的属性各不相同，用数组存储。 */
USTRUCT(BlueprintType)
struct FInv_ItemStatModifier
{
	GENERATED_BODY()

	/** 词条标识，例如 Item.Stat.AttackPower。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stats")
	FGameplayTag StatTag;

	/** 词条数值。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item|Stats")
	float Value = 0.0f;
};

/**
 * 指向另一个物品定义的数量条目，用于制作材料与分解产物。
 * 使用软引用避免资产之间的硬加载链；表格导入时由 ItemId 解析。
 */
USTRUCT(BlueprintType)
struct FInv_ItemAmount
{
	GENERATED_BODY()

	/** 物品定义资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item")
	TSoftObjectPtr<UInv_ItemDefinition> ItemDefinition = nullptr;

	/** 数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Item", meta = (ClampMin = 1))
	int32 Count = 1;
};
