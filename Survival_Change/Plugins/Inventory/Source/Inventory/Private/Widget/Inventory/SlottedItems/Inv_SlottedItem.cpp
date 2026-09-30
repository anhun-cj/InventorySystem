#include "Widget/Inventory/SlottedItems/Inv_SlottedItem.h"

#include "Blueprint/DragDropOperation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"

FReply UInv_SlottedItem::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	//OnSlottedItemClicked.Broadcast(GridIndex, MouseEvent);
	bIsMouseDown = true;
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UInv_SlottedItem::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsMouseDown)
	{
		bIsMouseDown = false;
	}
	return FReply::Handled();
}

void UInv_SlottedItem::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	UInv_InventoryStatics::ItemHovered(GetOwningPlayer(), InventoryItem.Get());
}

void UInv_SlottedItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
}

void UInv_SlottedItem::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	
	bIsMouseDown = false;
	
	UInv_HoverItem* HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	HoverItem->SetInventoryItem(InventoryItem.Get());
	InventoryGrid->SetHoverItem(HoverItem);
	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();
	
	DragOp->Payload = HoverItem;
	DragOp->DefaultDragVisual = HoverItem;
	OutOperation = DragOp;
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
}

bool UInv_SlottedItem::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
	APlayerController* PlayerController = GetOwningPlayer();
	UInv_InventoryComponent* IC = UInv_InventoryStatics::GetInventoryComponent(PlayerController);
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	IC->Server_MoveItem(HoverItem->GetInventoryItem(), InventoryGrid->GetResult(), InventoryGrid->GetItemDropIndex());
	return true;
}

void UInv_SlottedItem::SetInventoryItem(UInv_InventoryItem* Item)
{
	InventoryItem = Item;
}

void UInv_SlottedItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);
}

void UInv_SlottedItem::UpDateStackCount(int32 StackCount) const
{
	if (StackCount > 0)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
		Text_StackCount->SetText(FText::AsNumber(StackCount));
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UInv_SlottedItem::SetInventoryGrid(UInv_InventoryGrid* Grid)
{
	InventoryGrid = Grid;
}
