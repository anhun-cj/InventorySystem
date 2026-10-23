#include "Widget/Inventory/SlottedItems/Inv_SlottedItem.h"
#include "Widget/Inventory/GridSlot/Inv_GridSlot.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"

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
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"
#include "Widget/ItemDescription/Inv_ItemDescription.h"

FReply UInv_SlottedItem::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	bIsMouseDown = MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
	if (!bIsMouseDown)
		return FReply::Handled();
	return FReply::Handled().DetectDrag(TakeWidget(), EKeys::LeftMouseButton);
}

FReply UInv_SlottedItem::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bIsMouseDown = false;
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
		OnSlottedItemClicked.Broadcast(InventoryItem.Get());
	return FReply::Handled();
}

FReply UInv_SlottedItem::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		return Super::NativeOnMouseButtonDoubleClick(InGeometry, InMouseEvent);
	bIsMouseDown = false;
	UInv_SpatialInventory* Menu = Cast<UInv_SpatialInventory>(InventoryGrid->GetHoverSourceMenu());
	if (!Menu)
		return FReply::Unhandled();
	UInv_InventoryItem* Item = InventoryItem.Get();
	UInv_EquippedGridSlot* EquipmentSlot = nullptr;
	for (UInv_EquippedGridSlot* CandidateSlot : Menu->Equipments)
	{
		if (Item->GetItemManifest().GetItemType().MatchesTag(CandidateSlot->GetEquippedTag()))
		{
			EquipmentSlot = CandidateSlot;
			break;
		}
	}
	if (!EquipmentSlot)
		return FReply::Handled();

	InventoryGrid->CloseItemPopUp();
	SetToolTip(nullptr);
	UInv_InventoryItem* ItemToUnequip = EquipmentSlot->GetInventoryItem().Get();
	const int32 SourceIndex = GridIndex;
	UInv_InventoryComponent* SourceComponent = InventoryGrid->GetInventoryComponent();
	UInv_InventoryItem* SourceContainer = InventoryGrid->ContainerItem;
	const int32 SourceSubGridIndex = InventoryGrid->SubGridIndex;
	int32 ItemDropIndex = INDEX_NONE;
	int32 ItemDropSubGridIndex = 0;
	// 新装备即将离开库存，先腾出占格，再按快速卸装顺序查找旧装备的位置。
	InventoryGrid->RemoveItem(Item);
	if (ItemToUnequip)
	{
		for (UInv_InventoryGrid* Grid : {Menu->GetInventoryGrid(), Menu->BackPackInventory.Get(), Menu->ChestRigInventory.Get()})
		{
			if (Grid != Menu->GetInventoryGrid() && (!Grid->ContainerItem || Grid->ContainerItem == ItemToUnequip))
				continue;
			if (SourceComponent->AddItem(ItemToUnequip, ItemDropIndex, Grid, &ItemDropSubGridIndex))
				break;
		}
		if (ItemDropIndex == INDEX_NONE)
		{
			InventoryGrid->AddItemToIndices(Item, SourceIndex);
			return FReply::Handled();
		}
		Menu->UnEquipItem(ItemToUnequip);
	}
	Menu->EquipItem(Item);
	GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>()->Server_EquipItem(Item, ItemToUnequip,
		ItemDropIndex, Menu->GetInventoryGrid()->GetInventoryComponent(), SourceComponent,
		EquipmentSlot->GetEquippedTag(), SourceIndex, SourceContainer, SourceSubGridIndex, ItemDropSubGridIndex, true);
	return FReply::Handled();
}

void UInv_SlottedItem::NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
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

void UInv_SlottedItem::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	SetToolTip(nullptr);
	ItemDescription = nullptr;
}

void UInv_SlottedItem::NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent,
	UDragDropOperation*& OutOperation)
{
	bIsMouseDown = false;
	if (!InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
		return;

	Super::NativeOnDragDetected(InGeometry, InMouseEvent, OutOperation);
	SetToolTip(nullptr);
	ItemDescription = nullptr;

	UInv_InventoryItem* Item = InventoryItem.Get();
	if (!Item || !InventoryGrid.IsValid())
		return;

	UInv_HoverItem* HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	if (!HoverItem)
		return;
	HoverItem->SetInventoryItem(Item);
	HoverItem->SetCol(InventoryGrid->GetCol());
	HoverItem->SetInventoryGrid(InventoryGrid.Get());
	HoverItem->SetPreviousGridIndex(GridIndex);
	if (HoverItem->IsStackable())
		HoverItem->UpDateStackCount(InventoryGrid->GetStackAmount(InventoryGrid->GridSlots[GridIndex]));
	InventoryGrid->SetHoverItem(HoverItem);
	UDragDropOperation* DragOp = NewObject<UDragDropOperation>();

	DragOp->Payload = HoverItem;
	DragOp->DefaultDragVisual = HoverItem;
	OutOperation = DragOp;
}

bool UInv_SlottedItem::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	if (!HoverItem || !InventoryGrid.IsValid())
		return false;
	
	UInv_InventoryItem* ItemToTransfer = HoverItem->GetInventoryItem();
	if (!ItemToTransfer)
		return false;
	UInv_InventoryItem* TargetContainer = InventoryItem.Get();
	// 放到容器图标时发送自动存入请求；普通物品仍交给原网格落点逻辑处理。
	if (TargetContainer && TargetContainer->GridAsset)
	{
		UInv_InventoryBase* Menu = InventoryGrid->GetHoverSourceMenu();
		for (UInv_InventoryItem* Parent = TargetContainer; Parent; Parent = Parent->ParentContainerItem.Get())
			if (Parent == ItemToTransfer)
			{
				Menu->ClearHoverItem();
				return true;
			}
		UInv_InventoryGrid* SourceGrid = HoverItem->GetGrid();
		if (SourceGrid->ContainerItem == TargetContainer)
		{
			Menu->ClearHoverItem();
			return true;
		}
		const int32 SourceIndex = HoverItem->GetPreviousGridIndex();
		// 装备槽来源走卸装请求，网格来源走普通转移请求；最终空间由服务器确认。
		UInv_RequestComponent* Request = GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>();
		if (SourceIndex == INDEX_NONE)
		{
			Cast<UInv_SpatialInventory>(Menu)->UnEquipItem(ItemToTransfer);
			Request->Server_EquipItem(nullptr, ItemToTransfer, INDEX_NONE, SourceGrid->GetInventoryComponent(),
				InventoryGrid->GetInventoryComponent(), HoverItem->EquipmentType, INDEX_NONE, TargetContainer, 0, 0);
		}
		else
		{
			SourceGrid->RemoveItem(ItemToTransfer);
			Request->TransferItem(ItemToTransfer, FInv_SpaceQueryResult(), INDEX_NONE,
				InventoryGrid->GetInventoryComponent(), SourceGrid->GetInventoryComponent(), SourceIndex,
				TargetContainer, SourceGrid->ContainerItem, SourceGrid->SubGridIndex, InventoryGrid->SubGridIndex);
		}
		Menu->ClearHoverItem();
		return true;
	}
	
	const int32 DropIndex = InventoryGrid->GetItemDropIndex();
	if (DropIndex == INDEX_NONE)
	{
		InventoryGrid->GetHoverSourceMenu()->ClearHoverItem();
		return true;
	}
	return InventoryGrid->GridSlots[DropIndex]->NativeOnDrop(InGeometry, InDragDropEvent, InOperation);
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
