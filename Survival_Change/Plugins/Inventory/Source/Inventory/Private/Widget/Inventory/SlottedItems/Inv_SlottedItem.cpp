#include "Widget/Inventory/SlottedItems/Inv_SlottedItem.h"

#include "Blueprint/DragDropOperation.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/ItemDescription/Inv_ItemDescription.h"

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
	UInv_InventoryItem* Item = InventoryItem.Get();
	if (!Item)
		return;

	UInv_InventoryStatics::ItemHovered(InventoryGrid->GetInventoryComponent(), Item);
	ItemDescription = CreateWidget<UInv_ItemDescription>(GetOwningPlayer(), ItemDescriptionClass);
	if (!ItemDescription)
		return;
	Item->GetItemManifest().AssimilateInventoryFragment(ItemDescription);
	SetToolTip(ItemDescription);
}

void UInv_SlottedItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	SetToolTip(nullptr);
	ItemDescription = nullptr;
}

void UInv_SlottedItem::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	SetToolTip(nullptr);
	ItemDescription = nullptr;
	bIsMouseDown = false;

	UInv_InventoryItem* Item = InventoryItem.Get();
	if (!Item || !InventoryGrid.IsValid())
		return;

	UInv_HoverItem* HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	if (!HoverItem)
		return;
	HoverItem->SetInventoryItem(Item);
	HoverItem->SetCol(InventoryGrid->GetCol());
	InventoryGrid->SetHoverItem(HoverItem);
	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();

	DragOp->Payload = HoverItem;
	DragOp->DefaultDragVisual = HoverItem;
	OutOperation = DragOp;
}

bool UInv_SlottedItem::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
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

	UInv_InventoryComponent* SourceComp = ItemToTransfer->GetInventoryComponent();
	if (!IsValid(SourceComp))
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
