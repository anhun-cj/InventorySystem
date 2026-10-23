#include "Grid/Inv_InventoryGridAssetFactory.h"

#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"

UInv_InventoryGridAssetFactory::UInv_InventoryGridAssetFactory()
{
	SupportedClass = UInv_InventoryGridAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UInv_InventoryGridAssetFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name,
	EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UInv_InventoryGridAsset* Asset = NewObject<UInv_InventoryGridAsset>(InParent, Class, Name, Flags | RF_Transactional);
	Asset->Grids.Add(NewObject<UInv_InventoryGridConfig>(Asset, NAME_None, RF_Transactional));
	return Asset;
}
