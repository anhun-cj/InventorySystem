#include "Inv_ItemImportLibrary.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/DataTable.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/DataTableFactory.h"
#include "FileHelpers.h"
#include "GameplayTagsManager.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Items/Definitions/Inv_GameplayTagTableRow.h"
#include "Items/Definitions/Inv_ItemDefinition.h"
#include "Items/Definitions/Inv_ItemDefinitionTableRow.h"

namespace InvItemImport
{
	static bool IsNumericId(const FName Id)
	{
		const FString IdText = Id.ToString();
		if (Id.IsNone() || IdText.IsEmpty())
		{
			return false;
		}
		for (const TCHAR Character : IdText)
		{
			if (!FChar::IsDigit(Character))
			{
				return false;
			}
		}
		return true;
	}

	/** 把 /Game/... 资产路径或项目相对路径解析成磁盘绝对路径；绝对路径原样返回。 */
	static FString ResolveCsvPath(const FString& InputPath)
	{
		FString Path = InputPath.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			return Path;
		}

		// /Game/... → <Project>/Content/...
		if (Path.StartsWith(TEXT("/Game/")))
		{
			const FString Relative = Path.Mid(6);
			return FPaths::ProjectContentDir() / Relative;
		}

		// 相对路径 → 基于项目目录
		if (FPaths::IsRelative(Path))
		{
			return FPaths::ProjectDir() / Path;
		}

		return Path;
	}

	static bool AddImportProblem(const FName RowName, const FString& Message,
		TArray<FString>& OutImportProblems)
	{
		const FString Problem = FString::Printf(TEXT("Row '%s': %s"), *RowName.ToString(), *Message);
		OutImportProblems.Add(Problem);
		UE_LOG(LogTemp, Error, TEXT("%s"), *Problem);
		return false;
	}

	static void LogRowWarning(const FName RowName, const FString& Message)
	{
		UE_LOG(LogTemp, Warning, TEXT("Row '%s': %s"), *RowName.ToString(), *Message);
	}

	static bool ResolvePackageAndObjectPath(const FString& InputPath, FString& OutPackageName, FString& OutObjectPath)
	{
		FString Path = InputPath.TrimStartAndEnd();
		if (Path.IsEmpty())
		{
			return false;
		}

		if (Path.StartsWith(TEXT("/")) && Path.Contains(TEXT(".")))
		{
			OutObjectPath = Path;
			OutPackageName = FPackageName::ObjectPathToPackageName(Path);
		}
		else
		{
			OutPackageName = Path;
			const FString AssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
			OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *AssetName);
		}

		return FPackageName::IsValidLongPackageName(OutPackageName, true);
	}

	template <typename TObjectType>
	static TObjectType* LoadOptionalObject(const FString& InputPath)
	{
		if (InputPath.TrimStartAndEnd().IsEmpty())
		{
			return nullptr;
		}

		FString PackageName;
		FString ObjectPath;
		if (!ResolvePackageAndObjectPath(InputPath, PackageName, ObjectPath))
		{
			UE_LOG(LogTemp, Warning, TEXT("Invalid object path in item table: %s"), *InputPath);
			return nullptr;
		}

		return LoadObject<TObjectType>(nullptr, *ObjectPath);
	}

	static UClass* GetDefinitionClass(EInv_ItemDefinitionKind Kind)
	{
		switch (Kind)
		{
		case EInv_ItemDefinitionKind::Equipment:
			return UInv_EquipmentItemDefinition::StaticClass();
		case EInv_ItemDefinitionKind::Consumable:
			return UInv_ConsumableItemDefinition::StaticClass();
		default:
			return UInv_ItemDefinition::StaticClass();
		}
	}

	static FString MakeGeneratedItemPackageName(const FName ItemId)
	{
		return FString::Printf(TEXT("/Game/Items/DA_Item_%s"), *ItemId.ToString());
	}

	static bool FindExistingItemById(const FName ItemId, UInv_ItemDefinition*& OutItem, FString& OutError)
	{
		OutItem = nullptr;
		OutError.Reset();

		FAssetRegistryModule& AssetRegistryModule =
			FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		TArray<FAssetData> ItemAssets;
		AssetRegistryModule.Get().GetAssetsByClass(
			UInv_ItemDefinition::StaticClass()->GetClassPathName(), ItemAssets, true);

		for (const FAssetData& AssetData : ItemAssets)
		{
			UInv_ItemDefinition* Candidate = Cast<UInv_ItemDefinition>(AssetData.GetAsset());
			if (!Candidate || Candidate->ItemId != ItemId)
			{
				continue;
			}
			if (OutItem && OutItem != Candidate)
			{
				OutError = FString::Printf(
					TEXT("ItemId %s is used by multiple assets: %s and %s."),
					*ItemId.ToString(), *OutItem->GetPathName(), *Candidate->GetPathName());
				return false;
			}
			OutItem = Candidate;
		}
		return true;
	}

	static bool ResolveOrCreateItemDefinition(const FName ItemId, const EInv_ItemDefinitionKind Kind,
		UInv_ItemDefinition* PreferredItem, UInv_ItemDefinition*& OutItem,
		bool& bOutCreated, FString& OutError)
	{
		OutItem = nullptr;
		bOutCreated = false;
		OutError.Reset();

		UClass* RequiredClass = GetDefinitionClass(Kind);
		UInv_ItemDefinition* Item = PreferredItem;
		if (Item && !Item->ItemId.IsNone() && Item->ItemId != ItemId)
		{
			OutError = FString::Printf(TEXT("DataTable row points to %s, which belongs to ItemId %s."),
				*Item->GetPathName(), *Item->ItemId.ToString());
			return false;
		}

		if (!Item && !FindExistingItemById(ItemId, Item, OutError))
		{
			return false;
		}

		const FString GeneratedPackageName = MakeGeneratedItemPackageName(ItemId);
		const FString GeneratedAssetName = FPackageName::GetLongPackageAssetName(GeneratedPackageName);
		const FString GeneratedObjectPath = FString::Printf(
			TEXT("%s.%s"), *GeneratedPackageName, *GeneratedAssetName);
		if (!Item)
		{
			UObject* ObjectAtGeneratedPath = FindObject<UObject>(nullptr, *GeneratedObjectPath);
			if (!ObjectAtGeneratedPath)
			{
				FAssetRegistryModule& AssetRegistryModule =
					FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
				const FAssetData ExistingAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(
					FSoftObjectPath(GeneratedObjectPath));
				if (ExistingAssetData.IsValid())
				{
					ObjectAtGeneratedPath = ExistingAssetData.GetAsset();
				}
			}
			if (ObjectAtGeneratedPath)
			{
				Item = Cast<UInv_ItemDefinition>(ObjectAtGeneratedPath);
				if (!Item)
				{
					OutError = FString::Printf(TEXT("Generated item path is occupied by %s: %s"),
						*GetNameSafe(ObjectAtGeneratedPath->GetClass()), *GeneratedObjectPath);
					return false;
				}
				if (!Item->ItemId.IsNone() && Item->ItemId != ItemId)
				{
					OutError = FString::Printf(TEXT("Generated item path belongs to ItemId %s: %s"),
						*Item->ItemId.ToString(), *GeneratedObjectPath);
					return false;
				}
			}
		}

		if (Item && Item->GetClass() != RequiredClass)
		{
			OutError = FString::Printf(
				TEXT("ItemId %s requires %s but existing asset is %s: %s"),
				*ItemId.ToString(), *GetNameSafe(RequiredClass), *GetNameSafe(Item->GetClass()),
				*Item->GetPathName());
			return false;
		}

		if (!Item)
		{
			UPackage* Package = CreatePackage(*GeneratedPackageName);
			Item = NewObject<UInv_ItemDefinition>(Package, RequiredClass, *GeneratedAssetName,
				RF_Public | RF_Standalone | RF_Transactional);
			if (!Item)
			{
				OutError = FString::Printf(TEXT("Could not create item asset: %s"), *GeneratedObjectPath);
				return false;
			}
			FAssetRegistryModule::AssetCreated(Item);
			bOutCreated = true;
		}
		else
		{
			Item->Modify();
		}

		OutItem = Item;
		return true;
	}

	static bool ResolveGameplayTagId(const UDataTable* TagDataTable, const FName TagId,
		FGameplayTag& OutTag, FString& OutError)
	{
		OutTag = FGameplayTag();
		OutError.Reset();
		if (!TagDataTable || TagDataTable->GetRowStruct() != FInv_GameplayTagTableRow::StaticStruct())
		{
			OutError = TEXT("Tag DataTable must use FInv_GameplayTagTableRow.");
			return false;
		}
		if (!IsNumericId(TagId))
		{
			OutError = FString::Printf(TEXT("TagId must be numeric: %s"), *TagId.ToString());
			return false;
		}

		const FInv_GameplayTagTableRow* Row = TagDataTable->FindRow<FInv_GameplayTagTableRow>(
			TagId, TEXT("ResolveGameplayTagId"), false);
		if (!Row || !Row->bEnabled)
		{
			OutError = FString::Printf(TEXT("Unknown or disabled TagId: %s"), *TagId.ToString());
			return false;
		}

		OutTag = Row->GameplayTag.IsValid()
			? Row->GameplayTag
			: UGameplayTagsManager::Get().RequestGameplayTag(Row->TagName, false);
		if (!OutTag.IsValid())
		{
			OutError = FString::Printf(TEXT("TagId %s points to an unregistered GameplayTag: %s"),
				*TagId.ToString(), *Row->TagName.ToString());
			return false;
		}
		return true;
	}

	static bool ParseTags(const FString& Source, const UDataTable* TagDataTable,
		FGameplayTagContainer& OutTags, FString& OutError)
	{
		OutTags.Reset();
		OutError.Reset();
		if (Source.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}

		TArray<FString> Parts;
		Source.ParseIntoArray(Parts, TEXT("|"), false);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			FGameplayTag Tag;
			if (!ResolveGameplayTagId(TagDataTable, FName(*Part), Tag, OutError))
			{
				OutTags.Reset();
				return false;
			}
			OutTags.AddTag(Tag);
		}
		return true;
	}

	static bool ParseStats(const FString& Source, const UDataTable* TagDataTable,
		TArray<FInv_ItemStatModifier>& OutStats, FString& OutError)
	{
		OutStats.Reset();
		OutError.Reset();
		if (Source.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}

		TArray<FString> Parts;
		Source.ParseIntoArray(Parts, TEXT("|"), false);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			FString TagIdText;
			FString ValueText;
			if (!Part.Split(TEXT("&"), &TagIdText, &ValueText))
			{
				OutError = FString::Printf(
					TEXT("Entry must use TagId&Value, for example 20200&24: %s"), *Part);
				OutStats.Reset();
				return false;
			}

			TagIdText.TrimStartAndEndInline();
			ValueText.TrimStartAndEndInline();
			if (!ValueText.IsNumeric())
			{
				OutError = FString::Printf(TEXT("Value is not numeric: %s"), *Part);
				OutStats.Reset();
				return false;
			}

			FGameplayTag StatTag;
			if (!ResolveGameplayTagId(TagDataTable, FName(*TagIdText), StatTag, OutError))
			{
				OutStats.Reset();
				return false;
			}

			FInv_ItemStatModifier& Modifier = OutStats.AddDefaulted_GetRef();
			Modifier.StatTag = StatTag;
			Modifier.Value = FCString::Atof(*ValueText);
		}
		return true;
	}

	static bool ResolveEquipmentSlotTag(const FName TagId, const UDataTable* TagDataTable,
		FGameplayTag& OutTag, FString& OutError)
	{
		OutTag = FGameplayTag();
		OutError.Reset();
		if (TagId.IsNone())
		{
			return true;
		}
		return ResolveGameplayTagId(TagDataTable, TagId, OutTag, OutError);
	}

	/** 解析 ItemId&Count|ItemId&Count，通过 AssetRegistry 按 ItemId 找资产并保存软引用。 */
	static bool ParseItemAmounts(const FString& Source, TArray<FInv_ItemAmount>& OutAmounts, FString& OutError)
	{
		OutAmounts.Reset();
		OutError.Reset();
		if (Source.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}

		TArray<FString> Parts;
		Source.ParseIntoArray(Parts, TEXT("|"), false);
		for (FString Part : Parts)
		{
			Part.TrimStartAndEndInline();
			FString ItemIdText;
			FString CountText;
			if (!Part.Split(TEXT("&"), &ItemIdText, &CountText))
			{
				OutError = FString::Printf(
					TEXT("Entry must use ItemId&Count, for example 10001&2: %s"), *Part);
				OutAmounts.Reset();
				return false;
			}

			ItemIdText.TrimStartAndEndInline();
			CountText.TrimStartAndEndInline();
			const FName ItemId(*ItemIdText);
			if (!IsNumericId(ItemId))
			{
				OutError = FString::Printf(TEXT("Referenced ItemId must be numeric: %s"), *Part);
				OutAmounts.Reset();
				return false;
			}
			if (!CountText.IsNumeric())
			{
				OutError = FString::Printf(TEXT("Count is not numeric: %s"), *Part);
				OutAmounts.Reset();
				return false;
			}

			UInv_ItemDefinition* ReferencedItem = nullptr;
			FString FindError;
			if (!FindExistingItemById(ItemId, ReferencedItem, FindError))
			{
				OutError = FindError;
				OutAmounts.Reset();
				return false;
			}
			if (!ReferencedItem)
			{
				OutError = FString::Printf(TEXT("Referenced ItemId has no item asset yet: %s"), *ItemIdText);
				OutAmounts.Reset();
				return false;
			}

			FInv_ItemAmount& Amount = OutAmounts.AddDefaulted_GetRef();
			Amount.ItemDefinition = ReferencedItem;
			Amount.Count = FMath::Max(1, FCString::Atoi(*CountText));
		}
		return true;
	}

	static UDataTable* LoadOrCreateDataTable(const FString& DataTableAssetPath, UScriptStruct* RowStruct,
		const FString& KeyField, const TCHAR* LogLabel)
	{
		FString PackageName;
		FString ObjectPath;
		if (!ResolvePackageAndObjectPath(DataTableAssetPath, PackageName, ObjectPath))
		{
			UE_LOG(LogTemp, Error, TEXT("Invalid %s DataTable asset path: %s"), LogLabel, *DataTableAssetPath);
			return nullptr;
		}

		if (UDataTable* Existing = LoadObject<UDataTable>(nullptr, *ObjectPath))
		{
			if (Existing->GetRowStruct() != RowStruct)
			{
				UE_LOG(LogTemp, Error, TEXT("%s DataTable uses a different row struct: %s"), LogLabel, *ObjectPath);
				return nullptr;
			}
			Existing->ImportKeyField = KeyField;
			return Existing;
		}

		UPackage* Package = CreatePackage(*PackageName);
		UDataTableFactory* Factory = NewObject<UDataTableFactory>();
		Factory->Struct = RowStruct;
		const FName AssetName(*FPackageName::GetLongPackageAssetName(PackageName));
		UDataTable* Created = Cast<UDataTable>(Factory->FactoryCreateNew(
			UDataTable::StaticClass(), Package, AssetName, RF_Public | RF_Standalone | RF_Transactional, nullptr, GWarn));
		if (Created)
		{
			Created->ImportKeyField = KeyField;
			FAssetRegistryModule::AssetCreated(Created);
			Package->MarkPackageDirty();
		}
		return Created;
	}

	/** 只写入表格管理的字段；不触碰手动配置的 Icon、EquipActorClass 和 SocketAttachPoint。 */
	static void ApplyRowToItem(UInv_ItemDefinition* Item, FInv_ItemDefinitionTableRow& Row, const FName RowName,
		const FGameplayTagContainer& ParsedItemTags, const TArray<FInv_ItemStatModifier>& ParsedBaseStats,
		const FGameplayTag& ParsedEquipmentSlotTag, const TArray<FInv_ItemStatModifier>& ParsedConsumeEffects,
		const TArray<FInv_ItemAmount>& ParsedIngredients, const TArray<FInv_ItemAmount>& ParsedDecomposeResults)
	{
		Item->ItemId = RowName;
		Row.AssetPath = Item;
		Row.ItemTags = ParsedItemTags;
		Row.DecomposeResults = ParsedDecomposeResults;

		Item->DisplayName = Row.ItemName;
		Item->Description = Row.Description;
		Item->ItemTags = ParsedItemTags;
		Item->GridSize = Row.GridSize;
		Item->GridPadding = Row.GridPadding;
		Item->DecomposeResults = ParsedDecomposeResults;

		if (UInv_EquipmentItemDefinition* Equipment = Cast<UInv_EquipmentItemDefinition>(Item))
		{
			FInv_EquipmentItemDefinitionTableRow& EquipmentRow = static_cast<FInv_EquipmentItemDefinitionTableRow&>(Row);
			EquipmentRow.ItemStats = ParsedBaseStats;
			EquipmentRow.EquipmentSlot = ParsedEquipmentSlotTag;
			EquipmentRow.CraftingIngredients = ParsedIngredients;
			Equipment->BaseStats = ParsedBaseStats;
			Equipment->CraftingIngredients = ParsedIngredients;
			Equipment->EquipmentSlotTag = ParsedEquipmentSlotTag;
			Equipment->MaxStackSize = 1;
			Equipment->DefaultStackCount = 1;
		}
		else if (UInv_ConsumableItemDefinition* Consumable = Cast<UInv_ConsumableItemDefinition>(Item))
		{
			FInv_ConsumableItemDefinitionTableRow& ConsumableRow = static_cast<FInv_ConsumableItemDefinitionTableRow&>(Row);
			ConsumableRow.ConsumeEffectList = ParsedConsumeEffects;
			ConsumableRow.CraftingIngredients = ParsedIngredients;
			Consumable->ConsumeEffects = ParsedConsumeEffects;
			Consumable->CraftingIngredients = ParsedIngredients;
			Consumable->MaxStackSize = FMath::Max(1, ConsumableRow.MaxStackSize);
			Consumable->DefaultStackCount = FMath::Max(1, ConsumableRow.DefaultStackCount);
		}
		else
		{
			const FInv_CommonItemDefinitionTableRow& CommonRow = static_cast<FInv_CommonItemDefinitionTableRow&>(Row);
			Item->MaxStackSize = FMath::Max(1, CommonRow.MaxStackSize);
			Item->DefaultStackCount = FMath::Max(1, CommonRow.DefaultStackCount);
			Item->AcquisitionSources = CommonRow.AcquisitionSources;
		}

		Item->MarkPackageDirty();
	}
}

bool UInv_ItemImportLibrary::SyncItemAssetFromImportedRow(const UDataTable* SourceDataTable,
	FInv_ItemDefinitionTableRow& Row, const FName RowName, UDataTable* TagDataTable, const bool bSaveAsset,
	TArray<FString>& OutImportProblems)
{
	if (!Row.bEnabled)
	{
		return true;
	}
	if (!SourceDataTable || !SourceDataTable->GetRowStruct()
		|| !SourceDataTable->GetRowStruct()->IsChildOf(FInv_ItemDefinitionTableRow::StaticStruct())
		|| SourceDataTable->GetRowStruct() == FInv_ItemDefinitionTableRow::StaticStruct())
	{
		return InvItemImport::AddImportProblem(RowName,
			TEXT("Asset import requires an equipment, consumable or common item table."), OutImportProblems);
	}
	if (RowName.IsNone())
	{
		return InvItemImport::AddImportProblem(RowName, TEXT("ItemId cannot be empty."), OutImportProblems);
	}
	if (!InvItemImport::IsNumericId(RowName))
	{
		return InvItemImport::AddImportProblem(RowName,
			FString::Printf(TEXT("ItemId row key must be numeric, for example 10001: %s"), *RowName.ToString()),
			OutImportProblems);
	}
	if (!TagDataTable || TagDataTable->GetRowStruct() != FInv_GameplayTagTableRow::StaticStruct())
	{
		return InvItemImport::AddImportProblem(RowName,
			TEXT("A valid FInv_GameplayTagTableRow DataTable is required to resolve TagId values."),
			OutImportProblems);
	}

	const EInv_ItemDefinitionKind Kind = SourceDataTable->GetRowStruct()->IsChildOf(FInv_EquipmentItemDefinitionTableRow::StaticStruct())
		? EInv_ItemDefinitionKind::Equipment : SourceDataTable->GetRowStruct()->IsChildOf(FInv_ConsumableItemDefinitionTableRow::StaticStruct())
		? EInv_ItemDefinitionKind::Consumable : EInv_ItemDefinitionKind::Base;
	const FInv_EquipmentItemDefinitionTableRow* EquipmentRow = Kind == EInv_ItemDefinitionKind::Equipment
		? static_cast<const FInv_EquipmentItemDefinitionTableRow*>(&Row) : nullptr;
	const FInv_ConsumableItemDefinitionTableRow* ConsumableRow = Kind == EInv_ItemDefinitionKind::Consumable
		? static_cast<const FInv_ConsumableItemDefinitionTableRow*>(&Row) : nullptr;
	const FString CraftingIds = EquipmentRow ? EquipmentRow->CraftingIngredientIds : ConsumableRow ? ConsumableRow->CraftingIngredientIds : FString();
	FString ParseError;
	FGameplayTagContainer ParsedItemTags;
	TArray<FInv_ItemStatModifier> ParsedBaseStats;
	TArray<FInv_ItemStatModifier> ParsedConsumeEffects;
	FGameplayTag ParsedEquipmentSlotTag;
	if (!InvItemImport::ParseTags(Row.ItemTagIds, TagDataTable, ParsedItemTags, ParseError))
	{
		return InvItemImport::AddImportProblem(RowName,
			FString::Printf(TEXT("ItemTagIds: %s"), *ParseError), OutImportProblems);
	}
	if (EquipmentRow && !InvItemImport::ParseStats(EquipmentRow->BaseStats, TagDataTable, ParsedBaseStats, ParseError))
	{
		return InvItemImport::AddImportProblem(RowName,
			FString::Printf(TEXT("BaseStats: %s"), *ParseError), OutImportProblems);
	}
	if (ConsumableRow && !InvItemImport::ParseStats(ConsumableRow->ConsumeEffects, TagDataTable, ParsedConsumeEffects, ParseError))
	{
		return InvItemImport::AddImportProblem(RowName,
			FString::Printf(TEXT("ConsumeEffects: %s"), *ParseError), OutImportProblems);
	}
	if (EquipmentRow
		&& !InvItemImport::ResolveEquipmentSlotTag(
			EquipmentRow->EquipmentSlotTagId, TagDataTable, ParsedEquipmentSlotTag, ParseError))
	{
		return InvItemImport::AddImportProblem(RowName,
			FString::Printf(TEXT("EquipmentSlotTagId: %s"), *ParseError), OutImportProblems);
	}

	// 跨物品引用（制作材料/分解产物）在逐行导入时可能引用尚未生成的资产；
	// 这里尽力解析，失败仅记警告，批量同步末尾会再做一次完整解析。
	TArray<FInv_ItemAmount> ParsedIngredients;
	TArray<FInv_ItemAmount> ParsedDecomposeResults;
	FString CrossRefError;
	if (!InvItemImport::ParseItemAmounts(CraftingIds, ParsedIngredients, CrossRefError))
	{
		InvItemImport::LogRowWarning(RowName,
			FString::Printf(TEXT("CraftingIngredientIds unresolved (will retry on batch sync): %s"), *CrossRefError));
	}
	if (!InvItemImport::ParseItemAmounts(Row.DecomposeResultIds, ParsedDecomposeResults, CrossRefError))
	{
		InvItemImport::LogRowWarning(RowName,
			FString::Printf(TEXT("DecomposeResultIds unresolved (will retry on batch sync): %s"), *CrossRefError));
	}

	UInv_ItemDefinition* Item = nullptr;
	bool bCreated = false;
	FString ResolveError;
	if (!InvItemImport::ResolveOrCreateItemDefinition(
		RowName, Kind, Row.AssetPath, Item, bCreated, ResolveError))
	{
		return InvItemImport::AddImportProblem(RowName, ResolveError, OutImportProblems);
	}

	InvItemImport::ApplyRowToItem(Item, Row, RowName, ParsedItemTags, ParsedBaseStats,
		ParsedEquipmentSlotTag, ParsedConsumeEffects, ParsedIngredients, ParsedDecomposeResults);

	if (bSaveAsset)
	{
		TArray<UPackage*> PackagesToSave{ Item->GetPackage() };
		if (!UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true))
		{
			return InvItemImport::AddImportProblem(RowName,
				FString::Printf(TEXT("Could not save item asset: %s"), *Item->GetPathName()), OutImportProblems);
		}
	}
	return true;
}

bool UInv_ItemImportLibrary::SyncItemAssetsFromDataTable(UDataTable* DataTable, UDataTable* TagDataTable,
	const bool bSaveAssets)
{
	if (!DataTable || !DataTable->GetRowStruct()
		|| !DataTable->GetRowStruct()->IsChildOf(FInv_ItemDefinitionTableRow::StaticStruct())
		|| DataTable->GetRowStruct() == FInv_ItemDefinitionTableRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Sync requires a DataTable using FInv_ItemDefinitionTableRow."));
		return false;
	}
	if (!TagDataTable || TagDataTable->GetRowStruct() != FInv_GameplayTagTableRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Item sync requires a DataTable using FInv_GameplayTagTableRow."));
		return false;
	}

	bool bAllSucceeded = true;
	TArray<UPackage*> PackagesToSave;
	TArray<TPair<FName, FInv_ItemDefinitionTableRow*>> EnabledRows;

	for (const TPair<FName, uint8*>& Pair : DataTable->GetRowMap())
	{
		FInv_ItemDefinitionTableRow* Row = reinterpret_cast<FInv_ItemDefinitionTableRow*>(Pair.Value);
		if (!Row || !Row->bEnabled)
		{
			continue;
		}

		TArray<FString> RowProblems;
		if (!SyncItemAssetFromImportedRow(DataTable, *Row, Pair.Key, TagDataTable, false, RowProblems))
		{
			for (const FString& Problem : RowProblems)
			{
				UE_LOG(LogTemp, Error, TEXT("%s"), *Problem);
			}
			bAllSucceeded = false;
			continue;
		}

		EnabledRows.Add(TPair<FName, FInv_ItemDefinitionTableRow*>(Pair.Key, Row));
		PackagesToSave.AddUnique(Row->AssetPath->GetPackage());
	}

	// 二次解析跨物品引用：此时本表所有物品资产均已存在，前向引用可全部解析。
	for (const TPair<FName, FInv_ItemDefinitionTableRow*>& RowPair : EnabledRows)
	{
		FInv_ItemDefinitionTableRow* Row = RowPair.Value;
		UInv_ItemDefinition* Item = Row->AssetPath;
		if (!Item)
		{
			continue;
		}

		bool bChanged = false;
		TArray<FInv_ItemAmount> ParsedAmounts;
		FString CrossRefError;
		FInv_EquipmentItemDefinitionTableRow* EquipmentRow = Cast<UInv_EquipmentItemDefinition>(Item)
			? static_cast<FInv_EquipmentItemDefinitionTableRow*>(Row) : nullptr;
		FInv_ConsumableItemDefinitionTableRow* ConsumableRow = Cast<UInv_ConsumableItemDefinition>(Item)
			? static_cast<FInv_ConsumableItemDefinitionTableRow*>(Row) : nullptr;
		const FString CraftingIds = EquipmentRow ? EquipmentRow->CraftingIngredientIds : ConsumableRow ? ConsumableRow->CraftingIngredientIds : FString();
		if (InvItemImport::ParseItemAmounts(CraftingIds, ParsedAmounts, CrossRefError))
		{
			if (EquipmentRow)
			{
				EquipmentRow->CraftingIngredients = ParsedAmounts;
				CastChecked<UInv_EquipmentItemDefinition>(Item)->CraftingIngredients = ParsedAmounts;
			}
			else if (ConsumableRow)
			{
				ConsumableRow->CraftingIngredients = ParsedAmounts;
				CastChecked<UInv_ConsumableItemDefinition>(Item)->CraftingIngredients = ParsedAmounts;
			}
			bChanged = true;
		}
		else if (!CraftingIds.TrimStartAndEnd().IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Row '%s': CraftingIngredientIds: %s"),
				*RowPair.Key.ToString(), *CrossRefError);
			bAllSucceeded = false;
		}
		if (InvItemImport::ParseItemAmounts(Row->DecomposeResultIds, ParsedAmounts, CrossRefError))
		{
			Row->DecomposeResults = ParsedAmounts;
			Item->DecomposeResults = ParsedAmounts;
			bChanged = true;
		}
		else if (!Row->DecomposeResultIds.TrimStartAndEnd().IsEmpty())
		{
			UE_LOG(LogTemp, Error, TEXT("Row '%s': DecomposeResultIds: %s"),
				*RowPair.Key.ToString(), *CrossRefError);
			bAllSucceeded = false;
		}
		if (bChanged)
		{
			Item->MarkPackageDirty();
			PackagesToSave.AddUnique(Item->GetPackage());
		}
	}

	if (bSaveAssets && !PackagesToSave.IsEmpty())
	{
		DataTable->MarkPackageDirty();
		PackagesToSave.AddUnique(DataTable->GetPackage());
		bAllSucceeded &= UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, true);
	}

	UE_LOG(LogTemp, Display, TEXT("Inv item sync complete. Rows: %d, Success: %s"),
		EnabledRows.Num(), bAllSucceeded ? TEXT("true") : TEXT("false"));
	return bAllSucceeded;
}

bool UInv_ItemImportLibrary::ImportGameplayTagCsv(const FString& CsvFilePath,
	const FString& DataTableAssetPath, const bool bSaveAssets)
{
	const FString ResolvedPath = InvItemImport::ResolveCsvPath(CsvFilePath);
	FString CsvText;
	if (!FFileHelper::LoadFileToString(CsvText, *ResolvedPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not read GameplayTag CSV: %s (resolved: %s)"), *CsvFilePath, *ResolvedPath);
		return false;
	}

	UDataTable* DataTable = InvItemImport::LoadOrCreateDataTable(
		DataTableAssetPath, FInv_GameplayTagTableRow::StaticStruct(), TEXT("TagId"), TEXT("GameplayTag"));
	if (!DataTable)
	{
		return false;
	}

	DataTable->Modify();
	const TArray<FString> Problems = DataTable->CreateTableFromCSVString(CsvText);
	for (const FString& Problem : Problems)
	{
		UE_LOG(LogTemp, Error, TEXT("GameplayTag CSV import: %s"), *Problem);
	}
	if (!Problems.IsEmpty())
	{
		return false;
	}

	DataTable->ImportKeyField = TEXT("TagId");
	DataTable->AssetImportData->Update(FPaths::ConvertRelativePathToFull(ResolvedPath));
	DataTable->MarkPackageDirty();
	if (bSaveAssets)
	{
		TArray<UPackage*> DataTablePackage{ DataTable->GetPackage() };
		if (!UEditorLoadingAndSavingUtils::SavePackages(DataTablePackage, true))
		{
			return false;
		}
	}
	return true;
}

bool UInv_ItemImportLibrary::ImportCsvAndSync(const FString& CsvFilePath, const FString& DataTableAssetPath,
	const FString& TagDataTableAssetPath, const bool bSaveAssets, EInv_ItemDefinitionKind ItemKind)
{
	const FString ResolvedPath = InvItemImport::ResolveCsvPath(CsvFilePath);
	FString CsvText;
	if (!FFileHelper::LoadFileToString(CsvText, *ResolvedPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Could not read item CSV: %s (resolved: %s)"), *CsvFilePath, *ResolvedPath);
		return false;
	}

	UScriptStruct* RowStruct = ItemKind == EInv_ItemDefinitionKind::Equipment ? FInv_EquipmentItemDefinitionTableRow::StaticStruct()
		: ItemKind == EInv_ItemDefinitionKind::Consumable ? FInv_ConsumableItemDefinitionTableRow::StaticStruct()
		: FInv_CommonItemDefinitionTableRow::StaticStruct();
	UDataTable* DataTable = InvItemImport::LoadOrCreateDataTable(DataTableAssetPath, RowStruct, TEXT("ItemId"), TEXT("Item"));
	if (!DataTable)
	{
		return false;
	}
	DataTable->Modify();
	{
		const TArray<FString> Problems = DataTable->CreateTableFromCSVString(CsvText);
		for (const FString& Problem : Problems)
		{
			UE_LOG(LogTemp, Error, TEXT("Item CSV import: %s"), *Problem);
		}
		if (!Problems.IsEmpty())
		{
			return false;
		}
	}

	DataTable->AssetImportData->Update(FPaths::ConvertRelativePathToFull(ResolvedPath));
	DataTable->MarkPackageDirty();
	if (bSaveAssets)
	{
		TArray<UPackage*> DataTablePackage{ DataTable->GetPackage() };
		if (!UEditorLoadingAndSavingUtils::SavePackages(DataTablePackage, true))
		{
			return false;
		}
	}

	// 这里只读取并保存表格；用户查看或编辑后，点击表格工具栏“导入资产”才同步。
	return true;
}
