#include "Widget/OpenInventory/Inv_OpenInventoryWidget.h"

#include "Building/Inv_Building.h"
#include "Components/TextBlock.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"

UInv_InventoryGrid* UInv_OpenInventoryWidget::InitInventory(UInv_InventoryComponent* Inventory,
	UInv_InventoryBase* Menu, UCanvasPanel* Canvas)
{
	if (Cast<UInv_FindItemComponent>(Inventory))
	{
		Text_BuildingName->SetText(FText::FromString(TEXT("附近物品")));
	}
	else if (const AInv_Building* Building = Cast<AInv_Building>(Inventory->GetOwner()))
	{
		Text_BuildingName->SetText(FText::FromString(Building->GetBuildingName()));
	}
	else
	{
		const UInv_ItemComponent* ItemComponent = Inventory->GetOwner()->FindComponentByClass<UInv_ItemComponent>();
		const FInv_ItemManifest Manifest = ItemComponent->GetItemManifest();
		Text_BuildingName->SetText(Manifest.GetFragmentOfTypeWithTag<FInv_TextFragment>(FragmentTags::ItemNameFragment)->GetText());
	}
	Grid->SetOwningCanvasPanel(Canvas);
	Grid->SetHoverSourceMenu(Menu);
	Grid->BindInventoryEvent(Inventory);
	Grid->InitInventory();
	return Grid;
}
