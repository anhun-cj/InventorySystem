#include "Widget/Inventory/GridSlot/Inv_GridSlot.h"

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
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		return FReply::Handled();
	bIsMouseDown = true;
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
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	bIsMouseDown = false;
	
	if (InventoryItem.IsValid())
		return;
	
	UInv_HoverItem* HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	HoverItem->SetInventoryItem(InventoryItem.Get());
	
	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();
	DragOp->Payload = HoverItem;
	DragOp->DefaultDragVisual = HoverItem;
	OutOperation = DragOp;
}

bool UInv_GridSlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	APlayerController* PlayerController = GetOwningPlayer();
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	AInv_PlayerController* Controller = Cast<AInv_PlayerController>(PlayerController);
	if (!Controller || !HoverItem || !InventoryGrid.IsValid())
		return false;

	UInv_InventoryItem* ItemToTransfer = HoverItem->GetInventoryItem();
	if (!ItemToTransfer)
		return false;
	UInv_FindItemComponent* FindItemComponent = Cast<UInv_FindItemComponent>(ItemToTransfer->GetInventoryComponent());
	if (FindItemComponent == InventoryGrid->GetInventoryComponent())
	{
		FindItemComponent->Server_MoveItem(ItemToTransfer, InventoryGrid->GetResult(), InventoryGrid->GetItemDropIndex());
	}
	else if (UInv_RequestComponent* RequestComp = Controller->FindComponentByClass<UInv_RequestComponent>())
	{
		RequestComp->TransferItem(ItemToTransfer, InventoryGrid->GetResult(), InventoryGrid->GetItemDropIndex(), InventoryGrid->GetInventoryComponent());
	}
	InventoryGrid->GetHoverSourceMenu()->ClearHoverItem();
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
