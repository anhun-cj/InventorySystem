#include "Widget/Inventory/GridSlot/Inv_GridSlot.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"

#include "Blueprint/DragDropOperation.h"
#include "Items/Inv_InventoryItem.h"
#include "Components/Image.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/ItemPopUp/Inv_ItemPopUp.h"

void UInv_GridSlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	GridSlotHoverer.Broadcast(TileIndex, InMouseEvent);
}

void UInv_GridSlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	GridSlotUnhovered.Broadcast(TileIndex, InMouseEvent);
}

FReply UInv_GridSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bIsMouseDown = InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	if (!bIsMouseDown)
		return FReply::Handled();
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UInv_GridSlot::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsMouseDown)
	{
		bIsMouseDown = false;
		
	}
	/*GridSlotClicked.Broadcast(TileIndex, InMouseEvent);*/
	return FReply::Handled();
}

void UInv_GridSlot::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
                                         UDragDropOperation*& OutOperation)
{
	bIsMouseDown = false;
	if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		return;
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	
	UInv_HoverItem* HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	if (!HoverItem)
		return;
	HoverItem->SetInventoryItem(InventoryItem.Get());
	HoverItem->SetCol(InventoryGrid->GetCol());
	HoverItem->SetInventoryGrid(InventoryGrid.Get());
	HoverItem->SetPreviousGridIndex(UpperLeftIndex);
	if (HoverItem->IsStackable())
		HoverItem->UpDateStackCount(InventoryGrid->GetStackAmount(this));
	InventoryGrid->SetHoverItem(HoverItem);
	
	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();
	DragOp->Payload = HoverItem;
	DragOp->DefaultDragVisual = HoverItem;
	OutOperation = DragOp;
}

bool UInv_GridSlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	if (!HoverItem || !InventoryGrid.IsValid())
		return false;
	if (!HoverItem->GetInventoryItem())
		return false;
	
	InventoryGrid->SetHoverItem(HoverItem);
	if (HoverItem->GetPreviousGridIndex() == INDEX_NONE)
	{
		UInv_SpatialInventory* Menu = Cast<UInv_SpatialInventory>(InventoryGrid->GetHoverSourceMenu());
		UInv_EquippedGridSlot* EquipmentSlot = Menu->FindSlotWithEquippedItem(HoverItem->GetInventoryItem());
		if (!EquipmentSlot)
		{
			Menu->ClearHoverItem();
			return true;
		}
		UInv_InventoryGrid* TargetGrid = InventoryGrid.Get();
		for (UInv_InventoryItem* Parent = TargetGrid->ContainerItem; Parent; Parent = Parent->ParentContainerItem.Get())
			if (Parent == HoverItem->GetInventoryItem())
			{
				Menu->ClearHoverItem();
				return true;
			}
		const FInv_SpaceQueryResult Result = TargetGrid->GetResult();
		if ((!Result.bHasSpace && !Result.bAllowItemDown) || Result.ValidItems.Num() > 1)
		{
			Menu->ClearHoverItem();
			return true;
		}
		UInv_EquippedSlotItem* EquippedItem = EquipmentSlot->GetEquippedSlotItem();
		UInv_InventoryItem* ItemToTransfer = HoverItem->GetInventoryItem();
		UInv_InventoryItem* TargetItem = Result.ValidItems.IsEmpty() ? nullptr : Result.ValidItems[0].Get();
		Menu->UnEquipItem(ItemToTransfer);
		if (TargetItem)
		{
			TargetGrid->RemoveItem(TargetItem);
			EquipmentSlot->OnItemEquipped(TargetItem, HoverItem->GetGrid()->GetTileSize());
		}
		TargetGrid->AddItemToIndices(ItemToTransfer, TargetGrid->GetItemDropIndex());
		Menu->RemoveSlotItem(EquippedItem, TargetGrid);
		Menu->ClearHoverItem();
		return true;
	}
	InventoryGrid->AddItem();
	return true;
}

void UInv_GridSlot::SetInventoryItem(UInv_InventoryItem* Item)
{
	InventoryItem = Item;
}

void UInv_GridSlot::SetItemPopUp(UInv_ItemPopUp* PopUp)
{
	/*ItemPopUp = PopUp;
	ItemPopUp->SetGridIndex(GetTileIndex());
	ItemPopUp->OnNativeDestruct.AddUObject(this, &UInv_GridSlot::OnItemPopUpDestruct);*/
}

UInv_ItemPopUp* UInv_GridSlot::GetItemPopUp() const
{
	return ItemPopUp.Get();
}

void UInv_GridSlot::SetInventoryGrid(UInv_InventoryGrid* Grid)
{
	InventoryGrid = Grid;
}

void UInv_GridSlot::SetGreen() const
{
	Image_Green->SetVisibility(ESlateVisibility::Visible);
	Image_Red->SetVisibility(ESlateVisibility::Collapsed);
}

void UInv_GridSlot::SetRed() const
{
	Image_Green->SetVisibility(ESlateVisibility::Collapsed);
	Image_Red->SetVisibility(ESlateVisibility::Visible);
}

void UInv_GridSlot::SetNormal() const
{
	Image_Green->SetVisibility(ESlateVisibility::Collapsed);
	Image_Red->SetVisibility(ESlateVisibility::Collapsed);
}

void UInv_GridSlot::SetUnoccupiedTexture() const
{
	Image_GridSlot->SetBrush(Brush_Unoccupied);
}

void UInv_GridSlot::SetOccupiedTexture() const
{
	Image_GridSlot->SetBrush(Brush_Occupied);
}

void UInv_GridSlot::OnItemPopUpDestruct(UUserWidget* Menu)
{
	ItemPopUp.Reset();
}
