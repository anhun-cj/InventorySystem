#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inv_GameplayTagTableRow.generated.h"

/**
 * GameplayTag 数字编号表。CSV 第一列 TagId 作为 DataTable 行名，
 * 其他配置表只保存 TagId，不再重复填写容易拼错的标签字符串。
 */
USTRUCT(BlueprintType)
struct INVENTORY_API FInv_GameplayTagTableRow : public FTableRowBase
{
	GENERATED_BODY()

	virtual void OnPostDataImport(const UDataTable* InDataTable, FName InRowName,
		TArray<FString>& OutCollectedImportProblems) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Tag")
	bool bEnabled = true;

	/** 已在项目 GameplayTags 配置或原生代码中注册的完整标签名。 */
	UPROPERTY(meta = (HideFromDataTableEditorColumn))
	FName TagName = NAME_None;

	/** TagName 导入并校验后得到的真实 GameplayTag。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Resolved Gameplay Tag",
		meta = (DataTableImportOptional))
	FGameplayTag GameplayTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Tag")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Gameplay Tag")
	FText Description;
};
