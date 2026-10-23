#include "Grid/Inv_InventoryGridAssetActions.h"

#include "Grid/Inv_InventoryGridAssetEditor.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"

FText FInv_InventoryGridAssetActions::GetName() const
{
	return NSLOCTEXT("InventoryEditor", "GridAssetName", "库存网格布局");
}

UClass* FInv_InventoryGridAssetActions::GetSupportedClass() const
{
	return UInv_InventoryGridAsset::StaticClass();
}

void FInv_InventoryGridAssetActions::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		TSharedRef<FInv_InventoryGridAssetEditor> Editor = MakeShared<FInv_InventoryGridAssetEditor>();
		Editor->InitEditor(Mode, EditWithinLevelEditor, CastChecked<UInv_InventoryGridAsset>(Object));
	}
}
