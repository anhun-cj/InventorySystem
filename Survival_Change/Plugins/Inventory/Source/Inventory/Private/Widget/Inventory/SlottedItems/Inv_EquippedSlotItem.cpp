#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"

#include "Blueprint/DragDropOperation.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/ItemDescription/Inv_ItemDescription.h"

FReply UInv_EquippedSlotItem::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bIsMouseDown = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	if (!bIsMouseDown)
		return FReply::Handled();
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UInv_EquippedSlotItem::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsMouseDown)
	{
		bIsMouseDown = false;
	}
	return FReply::Handled();
}

FReply UInv_EquippedSlotItem::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		return FReply::Unhandled();
	bIsMouseDown = false;
	UInv_InventoryItem* Item = InventoryItem.Get();
	UInv_InventoryComponent* Inventory = InventoryGrid->GetInventoryComponent();
	UInv_SpatialInventory* Menu = Cast<UInv_SpatialInventory>(InventoryGrid->GetHoverSourceMenu());
	UInv_InventoryGrid* TargetGrid = InventoryGrid.Get();
	int32 ItemDropIndex = INDEX_NONE;
	int32 ItemDropSubGridIndex = 0;
	if (!Inventory->AddItem(Item, ItemDropIndex, TargetGrid, &ItemDropSubGridIndex))
	{
		for (UInv_InventoryGrid* Grid : {Menu->BackPackInventory.Get(), Menu->ChestRigInventory.Get()})
		{
			if (!Grid->ContainerItem || Grid->ContainerItem == Item)
				continue;
			if (Inventory->AddItem(Item, ItemDropIndex, Grid, &ItemDropSubGridIndex))
			{
				TargetGrid = Grid;
				break;
			}
		}
		if (ItemDropIndex == INDEX_NONE)
			return FReply::Handled();
	}
	TargetGrid = TargetGrid->GetSubGrid(ItemDropSubGridIndex);
	SetToolTip(nullptr);
	Menu->UnEquipItem(Item);
	GridSlot->SetNormal();
	GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>()->Server_EquipItem(nullptr, Item,
		ItemDropIndex, Inventory, TargetGrid->GetInventoryComponent(),
		EquipmentTag, INDEX_NONE, TargetGrid->ContainerItem, 0, ItemDropSubGridIndex);
	return FReply::Handled();
}

void UInv_EquippedSlotItem::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                                 UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	if (OutOperation)
		Cast<UInv_HoverItem>(OutOperation->Payload)->EquipmentType = EquipmentTag;
}

bool UInv_EquippedSlotItem::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	GridSlot->SetNormal();
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	if (!HoverItem || !InventoryGrid.IsValid())
		return false;
	
	UInv_InventoryItem* ItemToTransfer = HoverItem->GetInventoryItem();
	if (!ItemToTransfer)
		return false;
	if (HoverItem->GetPreviousGridIndex() == INDEX_NONE)
	{
		InventoryGrid->GetHoverSourceMenu()->ClearHoverItem();
		return true;
	}
	
	if (ItemToTransfer->GetItemManifest().GetItemType().MatchesTag(GridSlot->GetEquippedTag()))
	{
		HoverItem->GetGrid()->RemoveItem(ItemToTransfer);
		GridSlot->OnItemEquipped(ItemToTransfer, InventoryGrid->GetTileSize());
		InventoryGrid->GetHoverSourceMenu()->RemoveSlotItem(this, InventoryGrid.Get());
	}
	InventoryGrid->GetHoverSourceMenu()->ClearHoverItem();
	return true;
}

void UInv_EquippedSlotItem::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInv_InventoryItem* Item = InventoryItem.Get();
	if (!Item)
		return;

	if (!InventoryGrid.IsValid())
		return;
	UInv_InventoryStatics::ItemHovered(InventoryGrid->GetInventoryComponent(), Item);
	ItemDescription = CreateWidget<UInv_ItemDescription>(GetOwningPlayer(), ItemDescriptionClass);
	if (!ItemDescription)
		return;
	Item->GetItemManifest().AssimilateInventoryFragment(ItemDescription);
	SetToolTip(ItemDescription);
}

void UInv_EquippedSlotItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	SetToolTip(nullptr);
	ItemDescription = nullptr;
}

void UInv_EquippedSlotItem::SetGridSlot(UInv_EquippedGridSlot* InSlot)
{
	GridSlot = InSlot;
}
