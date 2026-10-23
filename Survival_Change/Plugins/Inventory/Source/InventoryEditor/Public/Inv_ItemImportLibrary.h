#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Items/Definitions/Inv_ItemDefinitionTableRow.h"
#include "Inv_ItemImportLibrary.generated.h"

class UDataTable;
struct FInv_ItemDefinitionTableRow;

/** 编辑器专用的道具表格导入与资产同步入口。 */
UCLASS()
class INVENTORYEDITOR_API UInv_ItemImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 由用户显式导入资产时调用；RowName 是 CSV 第一列 ItemId。 */
	static bool SyncItemAssetFromImportedRow(const UDataTable* SourceDataTable,
		FInv_ItemDefinitionTableRow& Row, FName RowName, UDataTable* TagDataTable,
		bool bSaveAsset, TArray<FString>& OutImportProblems);

	/** 按 ItemId 查找或自动创建道具资产，并将 DataTable 每一行同步到对应资产（含跨物品引用二次解析）。 */
	UFUNCTION(BlueprintCallable, Category = "Inv|Item Import")
	static bool SyncItemAssetsFromDataTable(UDataTable* DataTable, UDataTable* TagDataTable,
		bool bSaveAssets = true);

	/** 读取标签 CSV，创建或更新以 TagId 为行键的专属 GameplayTag DataTable。 */
	UFUNCTION(BlueprintCallable, Category = "Inv|Item Import")
	static bool ImportGameplayTagCsv(const FString& CsvFilePath, const FString& DataTableAssetPath,
		bool bSaveAssets = true);

	/** 读取 UTF-8 CSV，仅创建或更新 DataTable；点击表格工具栏“导入资产”后再同步物品资产。 */
	UFUNCTION(BlueprintCallable, Category = "Inv|Item Import", meta = (DisplayName = "导入物品 CSV 到表格"))
	static bool ImportCsvAndSync(const FString& CsvFilePath, const FString& DataTableAssetPath,
		const FString& TagDataTableAssetPath, bool bSaveAssets = true, EInv_ItemDefinitionKind ItemKind = EInv_ItemDefinitionKind::Base);
};
