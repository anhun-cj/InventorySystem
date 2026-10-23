#include "Items/Definitions/Inv_GameplayTagTableRow.h"

#include "GameplayTagsManager.h"

namespace
{
	bool IsNumericTagId(const FName TagId)
	{
		const FString IdText = TagId.ToString();
		if (TagId.IsNone() || IdText.IsEmpty())
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
}

void FInv_GameplayTagTableRow::OnPostDataImport(const UDataTable* InDataTable, const FName InRowName,
	TArray<FString>& OutCollectedImportProblems)
{
	if (!bEnabled)
	{
		return;
	}

	auto AddProblem = [&OutCollectedImportProblems, InRowName](const FString& Message)
	{
		OutCollectedImportProblems.Add(FString::Printf(TEXT("Row '%s': %s"), *InRowName.ToString(), *Message));
	};

	if (!IsNumericTagId(InRowName))
	{
		AddProblem(TEXT("TagId must be a non-empty numeric row key, for example 20000."));
	}
	if (TagName.IsNone())
	{
		AddProblem(TEXT("TagName cannot be empty."));
		return;
	}
	GameplayTag = UGameplayTagsManager::Get().RequestGameplayTag(TagName, false);
	if (!GameplayTag.IsValid())
	{
		AddProblem(FString::Printf(TEXT("TagName is not registered in the project: %s"), *TagName.ToString()));
	}

	if (!InDataTable || InDataTable->GetRowStruct() != StaticStruct())
	{
		AddProblem(TEXT("Tag import requires FInv_GameplayTagTableRow."));
	}
}
