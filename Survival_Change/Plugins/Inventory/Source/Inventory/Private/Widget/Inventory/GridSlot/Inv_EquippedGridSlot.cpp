// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/GridSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"

void UInv_EquippedGridSlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	
}

void UInv_EquippedGridSlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	
}

void UInv_EquippedGridSlot::NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	Super::NativeOnDragEnter(InGeometry, InDragDropEvent, InOperation);
	UInv_HoverItem* HoverItem = InventoryGrid->GetHoverSourceMenu()->GetHoverItem();
	if (IsValid(HoverItem))
	{
		if (HoverItem->GetItemType().MatchesTag(EquipmentType))
		{
			SetGreen();
		}
		else
		{
			SetRed();
		}
	}
}

void UInv_EquippedGridSlot::NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation)
{
	SetNormal();
}

FReply UInv_EquippedGridSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	
	return FReply::Handled();
}

bool UInv_EquippedGridSlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	SetNormal();
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
	
	if (ItemToTransfer->GetItemManifest().GetItemType().MatchesTag(EquipmentType))
	{
		UInv_EquippedSlotItem* PreviousItem = EquippedSlotItem;
		HoverItem->GetGrid()->RemoveItem(ItemToTransfer);
		OnItemEquipped(ItemToTransfer, InventoryGrid->GetTileSize());
		InventoryGrid->GetHoverSourceMenu()->RemoveSlotItem(PreviousItem, InventoryGrid.Get());
	}
	InventoryGrid->GetHoverSourceMenu()->ClearHoverItem();
	return true;
}

UInv_EquippedSlotItem* UInv_EquippedGridSlot::OnItemEquipped(UInv_InventoryItem* Item,
                                                             float TileSize)
{
	if (EquippedSlotItem)
		EquippedSlotItem->RemoveFromParent();
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	if (!GridFragment)
		return nullptr;
	
	// 画刷只提供物品占格比例，实际显示大小由装备槽内的 ScaleBox 随布局适配。
	const FVector2D DrawSize = FVector2D(GridFragment->GetGridSize()) * TileSize;
	
	EquippedSlotItem = CreateWidget<UInv_EquippedSlotItem>(GetOwningPlayer(), EquippedSlotItemClass);
	EquippedSlotItem->SetInventoryItem(Item);
	EquippedSlotItem->SetGridIndex(INDEX_NONE);
	EquippedSlotItem->SetEquipmentTag(EquipmentType);
	EquippedSlotItem->SetGridSlot(this);
	EquippedSlotItem->SetInventoryGrid(InventoryGrid.Get());
	EquippedSlotItem->UpDateStackCount(0);
	SetInventoryItem(Item);
	const FInv_ImageFragment* ImageFragment = GetFragment<FInv_ImageFragment>(Item, FragmentTags::IconFragment);
	if (!ImageFragment)
		return nullptr;
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = DrawSize;
	
	EquippedSlotItem->SetImageBrush(Brush);
	
	Overlay_Root->AddChildToOverlay(EquippedSlotItem);
	
	if (UOverlaySlot* OverlaySlot = UWidgetLayoutLibrary::SlotAsOverlaySlot(EquippedSlotItem))
	{
		OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
		OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		OverlaySlot->SetPadding(FMargin(GridFragment->GetGridPadding()));
	}
	
	return EquippedSlotItem;
}
