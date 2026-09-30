#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_ItemFragment.h"

void UInv_HoverItem::SetImageBrush(const FSlateBrush& Brush) const
{
	Image_Icon->SetBrush(Brush);	
}

void UInv_HoverItem::UpDateStackCount(const int32 Count)
{
	StackCount = Count;
	if (Count > 0)
	{
		Text_StackCount->SetText(FText::AsNumber(Count));
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

FGameplayTag UInv_HoverItem::GetItemType() const
{
	if (InventoryItem.IsValid())
	{
		return InventoryItem->GetItemManifest().GetItemType();
	}
	return FGameplayTag();
}

void UInv_HoverItem::SetIsStackable(bool bStackable)
{
	bIsStackable = bStackable;
	if (!bIsStackable)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

UInv_InventoryItem* UInv_HoverItem::GetInventoryItem() const
{
	return InventoryItem.Get();
}

void UInv_HoverItem::SetInventoryItem(UInv_InventoryItem* Item)
{
	InventoryItem = Item;
	if (!InventoryItem.IsValid())
		return;
	bIsStackable = Item->IsStackable();
	/*if (!bIsStackable)
	{
		Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		StackCount = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>()->GetStackCount();
		Text_StackCount->SetText(FText::AsNumber(StackCount));
		Text_StackCount->SetVisibility(ESlateVisibility::Visible);
	}*/
	Text_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	const FInv_ImageFragment* ImageFragment = InventoryItem->GetItemManifest().GetFragmentOfType<FInv_ImageFragment>();
	const FInv_GridFragment* GridFragment = InventoryItem->GetItemManifest().GetFragmentOfType<FInv_GridFragment>();
	GridDimensions = GridFragment->GetGridSize();
	
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	FVector2D DrawSize = {ImageFragment->GetIconDimension().X * GridDimensions.X,ImageFragment->GetIconDimension().Y * GridDimensions.Y };
	Brush.ImageSize = DrawSize;
	Image_Icon->SetBrush(Brush);
}
