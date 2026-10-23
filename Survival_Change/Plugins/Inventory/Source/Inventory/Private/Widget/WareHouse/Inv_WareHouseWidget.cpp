#include "Widget/WareHouse/Inv_WareHouseWidget.h"

#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetTree.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"


FReply UInv_WareHouseWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		UnBindEvent();
		WareHouseInventory->ToggleInventoryMenu();
		return FReply::Handled();
	}
	
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UInv_WareHouseWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	SetIsFocusable(true);
	
	
	Survival_ActorEquipment->WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UInv_EquippedGridSlot* EquippedGridSlot = Cast<UInv_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			Equipments.Add(EquippedGridSlot);
			EquippedGridSlot->SetInventoryGrid(PlayerInventoryGrid);
			EquippedGridSlot->SetNormal();
		}
	});
}

UInv_HoverItem* UInv_WareHouseWidget::GetHoverItem() const
{
	if (PlayerInventoryGrid->HasHoveredItem())
		return PlayerInventoryGrid->GetHoverItem();
	if (WareHouseInventoryGrid->HasHoveredItem())
		return WareHouseInventoryGrid->GetHoverItem();
	if (Grid_Other->HasHoveredItem())
		return Grid_Other->GetHoverItem();
	return nullptr;
}

bool UInv_WareHouseWidget::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	APlayerController* PlayerController = GetOwningPlayer();
	AInv_PlayerController* Controller = Cast<AInv_PlayerController>(PlayerController);
	UInv_RequestComponent* RequestComp = Controller->FindComponentByClass<UInv_RequestComponent>();
	
	if (Cast<UInv_FindItemComponent>(HoverItem->GetInventoryItem()->GetInventoryComponent()))
	{
		ClearHoverItem();
		return true;
	}
	
	RequestComp->DropItem(HoverItem->GetInventoryItem());
	ClearHoverItem();
	return true;
}

void UInv_WareHouseWidget::InitWareHouseWidget(UInv_InventoryComponent* PlayerInventoryComponent,
                                               UInv_InventoryComponent* WareHouseInventoryComponent)
{
	PlayerInventory = PlayerInventoryComponent;
	WareHouseInventory = WareHouseInventoryComponent;
	OtherInventory = GetOwningPlayer()->FindComponentByClass<UInv_FindItemComponent>();
	
	PlayerInventory->OnItemEquipped.AddDynamic(this, &UInv_WareHouseWidget::EquipItem);
	PlayerInventory->OnItemUnequipped.AddDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
	
	WareHouseInventory->OnItemEquipped.AddDynamic(this, &UInv_WareHouseWidget::EquipItem);
	WareHouseInventory->OnItemUnequipped.AddDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
	
	OtherInventory->OnItemEquipped.AddDynamic(this, &UInv_WareHouseWidget::EquipItem);
	OtherInventory->OnItemUnequipped.AddDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
	
	PlayerInventoryGrid->BindInventoryEvent(PlayerInventory.Get());
	PlayerInventoryGrid->InitInventory();
	WareHouseInventoryGrid->BindInventoryEvent(WareHouseInventory.Get());
	WareHouseInventoryGrid->InitInventory();
	Grid_Other->BindInventoryEvent(OtherInventory.Get());
	Grid_Other->InitInventory();
	
	PlayerInventoryGrid->SetHoverSourceMenu(this);
	WareHouseInventoryGrid->SetHoverSourceMenu(this);
	Grid_Other->SetHoverSourceMenu(this);
}

UInv_InventoryGrid* UInv_WareHouseWidget::GetOtherGrid() const
{
	return Grid_Other;
}

void UInv_WareHouseWidget::UnBindEvent() const
{
	PlayerInventoryGrid->UnBindInventoryEvent();
	WareHouseInventoryGrid->UnBindInventoryEvent();
	Grid_Other->UnBindInventoryEvent();
	
	PlayerInventory->OnItemEquipped.RemoveDynamic(this, &UInv_WareHouseWidget::EquipItem);
	PlayerInventory->OnItemUnequipped.RemoveDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
	
	WareHouseInventory->OnItemEquipped.RemoveDynamic(this, &UInv_WareHouseWidget::EquipItem);
	WareHouseInventory->OnItemUnequipped.RemoveDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
	
	OtherInventory->OnItemEquipped.RemoveDynamic(this, &UInv_WareHouseWidget::EquipItem);
	OtherInventory->OnItemUnequipped.RemoveDynamic(this, &UInv_WareHouseWidget::UnEquipItem);
}

void UInv_WareHouseWidget::ClearHoverItem() const
{
	PlayerInventoryGrid->SetHoverItem(nullptr);
	WareHouseInventoryGrid->SetHoverItem(nullptr);
	Grid_Other->SetHoverItem(nullptr);
}

void UInv_WareHouseWidget::EquipItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	UInv_EquippedGridSlot* EquippedGridSlot = FindSlotWithEquippedType(Item->GetItemManifest().GetItemType());
	const float TileSize = UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UInv_EquippedSlotItem* EquippedSlotItem = EquippedGridSlot->OnItemEquipped(Item, TileSize);
	EquippedSlotItem->SetInventoryGrid(PlayerInventoryGrid);
}

void UInv_WareHouseWidget::UnEquipItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	UInv_EquippedGridSlot* GridSlot = FindSlotWithEquippedItem(Item);
	if (!IsValid(GridSlot))
		return;
	RemoveEquippedSlottedItem(GridSlot->GetEquippedSlotItem());
	ClearSlotItem(GridSlot);
}

void UInv_WareHouseWidget::ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot) const
{
	if (!IsValid(EquippedGridSlot))
		return;
	EquippedGridSlot->SetEquippedSlotItem(nullptr);
	EquippedGridSlot->SetInventoryItem(nullptr);
}

void UInv_WareHouseWidget::RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem) const
{
	if (!IsValid(EquippedSlotItem))
		return;
	EquippedSlotItem->RemoveFromParent();
}

UInv_EquippedGridSlot* UInv_WareHouseWidget::FindSlotWithEquippedItem(UInv_InventoryItem* Item) const
{
	auto* FoundEquippedGridSlot = Equipments.FindByPredicate([Item](UInv_EquippedGridSlot* EquippedGridSlot)
	{
		return EquippedGridSlot->GetInventoryItem() == Item;
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

UInv_EquippedGridSlot* UInv_WareHouseWidget::FindSlotWithEquippedType(const FGameplayTag& Tag) const
{
	auto* FoundEquippedGridSlot = Equipments.FindByPredicate([&Tag](UInv_EquippedGridSlot* EquippedGridSlot)
	{
		return EquippedGridSlot->GetEquippedTag().MatchesTag(Tag);
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}


