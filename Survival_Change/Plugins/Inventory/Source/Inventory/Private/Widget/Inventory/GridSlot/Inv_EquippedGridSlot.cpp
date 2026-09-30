// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/GridSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"

void UInv_EquippedGridSlot::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!IsAvailable())
		return;
	UInv_HoverItem* HoverItem = UInv_InventoryStatics::GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem))
		return;
	if (HoverItem->GetItemType().MatchesTag(EquipmentType))
	{
		SetOccupiedTexture();
	}
}

void UInv_EquippedGridSlot::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	if (!IsAvailable())
		return;
	UInv_HoverItem* HoverItem = UInv_InventoryStatics::GetHoverItem(GetOwningPlayer());
	if (!IsValid(HoverItem))
		return;
	if (IsValid(EquippedSlotItem))
		return;
	if (HoverItem->GetItemType().MatchesTag(EquipmentType))
	{
		SetUnoccupiedTexture();
	}
}

FReply UInv_EquippedGridSlot::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	EquippedGridSlotClicked.Broadcast(this, EquipmentType);
	return FReply::Handled();
}

bool UInv_EquippedGridSlot::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	APlayerController* PlayerController = GetOwningPlayer();
	UInv_InventoryComponent* IC = UInv_InventoryStatics::GetInventoryComponent(PlayerController);
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	
	return true;
}

UInv_EquippedSlotItem* UInv_EquippedGridSlot::OnItemEquipped(UInv_InventoryItem* Item, const FGameplayTag& Tag,
                                                             float TileSize)
{
	if (!EquipmentType.MatchesTagExact(Tag))
		return nullptr;
	
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	if (!GridFragment)
		return nullptr;
	const FIntPoint& Dimensions = GridFragment->GetGridSize();
	
	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	const FVector2D DrawSize = Dimensions * IconTileWidth;
	
	EquippedSlotItem = CreateWidget<UInv_EquippedSlotItem>(GetOwningPlayer(), EquippedSlotItemClass);
	EquippedSlotItem->SetInventoryItem(Item);
	EquippedSlotItem->SetEquipmentTag(Tag);
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
	
	FGeometry OverLayGeometry = Overlay_Root->GetCachedGeometry();
	auto OverLayPos = OverLayGeometry.Position;
	auto OverLaySize = OverLayGeometry.Size;
	const float LeftPadding = OverLaySize.X / 2 - DrawSize.X / 2;
	const float TopPadding = OverLaySize.Y / 2 - DrawSize.Y / 2;
	
	if (UOverlaySlot* OverlaySlot = UWidgetLayoutLibrary::SlotAsOverlaySlot(EquippedSlotItem))
	{
		OverlaySlot->SetPadding(FMargin(LeftPadding, TopPadding));
	}
	
	return EquippedSlotItem;
}
