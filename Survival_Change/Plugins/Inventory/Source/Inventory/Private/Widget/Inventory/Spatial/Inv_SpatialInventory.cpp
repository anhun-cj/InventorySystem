#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"
#include "Components/WidgetSwitcher.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Components/Button.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Widget/ItemDescription/Inv_ItemDescription.h"
#include "Inventory.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Blueprint/WidgetTree.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"

void UInv_SpatialInventory::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	/*Button_Equippable->OnClicked.AddDynamic(this, &ThisClass::ShowAll);
	Button_Consumable->OnClicked.AddDynamic(this, &ThisClass::ShowConsumable);
	Button_Craftable->OnClicked.AddDynamic(this, &ThisClass::ShowCraftable);*/

	Grid_All->SetOwningCanvasPanel(CanvasPanel);
	Grid_All->SetHoverSourceMenu(this);
	Grid_Other->SetHoverSourceMenu(this);
	
	Survival_ActorEquipment->WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UInv_EquippedGridSlot* EquippedGridSlot = Cast<UInv_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			Equipments.Add(EquippedGridSlot);
			EquippedGridSlot->SetInventoryGrid(Grid_All);
			EquippedGridSlot->SetNormal();
			EquippedGridSlot->EquippedGridSlotClicked.AddDynamic(this, &UInv_SpatialInventory::EquippedGridSlotClicked);
		}
	});
}

void UInv_SpatialInventory::EquippedGridSlotClicked(UInv_EquippedGridSlot* EquippedGridSlot,
	const FGameplayTag& EquipmentTag)
{
	if (!CanEquipHoverItem(EquippedGridSlot, EquipmentTag))
		return;

	UInv_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem) || !IsValid(HoverItem->GetInventoryItem()))
		return;

	// 在 ClearHoverItem 之前缓存物品指针
	UInv_InventoryItem* ItemToEquip = HoverItem->GetInventoryItem();

	const float TileSize = UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UInv_EquippedSlotItem* EquippedSlotItem = EquippedGridSlot->OnItemEquipped(ItemToEquip, EquipmentTag, TileSize);
	if (!IsValid(EquippedSlotItem))
		return;
	EquippedSlotItem->OnEquippedGridSlotItemClicked.AddDynamic(this, &UInv_SpatialInventory::EquippedSlottedItemClicked);
	
	if (!InventoryComponent.Get())
		return;

	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, nullptr);
	// 不要本地广播 OnItemEquipped：服务器 Multicast 在单机/主机/客户端都会执行，
	// 本地再广播一次会导致 OnEquip（如"力量增加"）执行两次
	
}

void UInv_SpatialInventory::EquippedSlottedItemClicked(UInv_EquippedSlotItem* EquippedSlotItem)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	
	if (IsValid(GetHoverItem()) && GetHoverItem()->IsStackable())
		return;
	UInv_InventoryItem* ItemToEquip = IsValid(GetHoverItem()) ? GetHoverItem()->GetInventoryItem() : nullptr;
	UInv_InventoryItem* ItemToUnequip = EquippedSlotItem->GetInventoryItem();
	
	UInv_EquippedGridSlot* GridSlot = FindSlotWithEquippedItem(ItemToUnequip);
	
	ClearSlotItem(GridSlot);
	
	Grid_All->AssignHoverItem(ItemToUnequip);
	RemoveEquippedSlottedItem(EquippedSlotItem);
	
	MakeEquippedSlottedItem(EquippedSlotItem, GridSlot, ItemToEquip);
	
	BroadcastSlotClickedDelegates(ItemToEquip, ItemToUnequip);
}

FReply UInv_SpatialInventory::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

bool UInv_SpatialInventory::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	APlayerController* PlayerController = GetOwningPlayer();
	AInv_PlayerController* Controller = Cast<AInv_PlayerController>(PlayerController);
	UInv_RequestComponent* RequestComp = Controller->FindComponentByClass<UInv_RequestComponent>();
	
	if (UInv_FindItemComponent* FindItemComponent = Cast<UInv_FindItemComponent>(HoverItem->GetInventoryItem()->GetInventoryComponent()))
	{
		FInv_SpaceQueryResult Result;
		FindItemComponent->Server_MoveItem(HoverItem->GetInventoryItem(), Result, 0);
		ClearHoverItem();
		return true;
	}
	
	RequestComp->DropItem(HoverItem->GetInventoryItem());
	ClearHoverItem();
	return true;
}

FReply UInv_SpatialInventory::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::B)
	{
		InventoryComponent->ToggleInventoryMenu();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry,InKeyEvent);
}

FInv_SlotAvailabilityResult UInv_SpatialInventory::HasRoomForItem(UInv_ItemComponent* ItemComponent) const
{
	return Grid_All->HasRoomForItem(ItemComponent);
}

void UInv_SpatialInventory::OnItemHovered(UInv_InventoryItem* InventoryItem)
{
	/*const FInv_ItemManifest& Manifest = InventoryItem->GetItemManifest();
	
	UInv_ItemDescription* Description = GetItemDescription();
	Description->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);
	
	FTimerDelegate DescriptionTimerDelegate;
	DescriptionTimerDelegate.BindLambda([this, &Manifest, Description]
	{
		Manifest.AssimilateInventoryFragment(Description);
		GetItemDescription()->SetVisibility(ESlateVisibility::HitTestInvisible);
	});
	
	GetOwningPlayer()->GetWorldTimerManager().SetTimer(DescriptionTimer, DescriptionTimerDelegate, DescriptionTimerDelay, false);*/
}

void UInv_SpatialInventory::OnItemUnhovered()
{
	/*GetItemDescription()->SetVisibility(ESlateVisibility::Collapsed);
	GetOwningPlayer()->GetWorldTimerManager().ClearTimer(DescriptionTimer);*/
}

bool UInv_SpatialInventory::HasHoveredItem() const
{
	if (Grid_All->HasHoveredItem())
		return true;
	return false;
}

UInv_HoverItem* UInv_SpatialInventory::GetHoverItem() const
{
	if (Grid_All->HasHoveredItem())
		return Grid_All->GetHoverItem();
	if (Grid_Other->HasHoveredItem())
		return Grid_Other->GetHoverItem();
	return nullptr;
}

float UInv_SpatialInventory::GetTileSize() const
{
	return Grid_All->GetTileSize();
}

void UInv_SpatialInventory::SetInventoryComponent(UInv_InventoryComponent* Inventory)
{
	InventoryComponent = Inventory;
	InventoryBindEvent();
}

void UInv_SpatialInventory::ShowAll()
{
	//SetActiveGrid(Grid_All, Button_Equippable);
}

void UInv_SpatialInventory::ShowConsumable()
{
}

void UInv_SpatialInventory::ShowCraftable()
{
}

void UInv_SpatialInventory::DisableButton(UButton* Button) const
{
	Button->SetIsEnabled(false);
}

void UInv_SpatialInventory::SetActiveGrid(UInv_InventoryGrid* Grid, UButton* Button)
{
	/*if (ActiveGrid.IsValid())
	{
		ActiveGrid->HideCursor();
		ActiveGrid->OnHide();
	}
	ActiveGrid = Grid;
	if (ActiveGrid.IsValid())
		ActiveGrid->ShowCursor();
	DisableButton(Button);
	Switcher->SetActiveWidget(Grid);*/
}

void UInv_SpatialInventory::SetItemDescriptionSizeAndPosition(UInv_ItemDescription* Description, UCanvasPanel* Canvas) const
{
	/*UCanvasPanelSlot* ItemDescriptionCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(Description);
	if (!IsValid(ItemDescriptionCPS))
		return;
	const FVector2D ItemDescriptionSize = Description->GetBoxSize();
	ItemDescriptionCPS->SetSize(ItemDescriptionSize);
	
	FVector2D ClampedPosition = UInv_WidgetUtils::GetClampedWidgetPosition(UInv_WidgetUtils::GetWidgetSize(Canvas), ItemDescriptionSize, UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer()));
	ItemDescriptionCPS->SetPosition(ClampedPosition);*/
}

bool UInv_SpatialInventory::CanEquipHoverItem(UInv_EquippedGridSlot* Equipment, const FGameplayTag& Tag) const
{
	if (!IsValid(Equipment) || Equipment->GetInventoryItem().IsValid())
		return false;
	UInv_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem))
		return false;
	
	UInv_InventoryItem* Item = HoverItem->GetInventoryItem();
	return HasHoveredItem() && IsValid(Item) && !HoverItem->IsStackable() && Item->GetItemManifest().GetItemCategory() == EInv_ItemCategory::Equippable && Item->GetItemManifest().GetItemType().MatchesTagExact(Tag);
}

UInv_EquippedGridSlot* UInv_SpatialInventory::FindSlotWithEquippedItem(UInv_InventoryItem* Item) const
{
	auto* FoundEquippedGridSlot = Equipments.FindByPredicate([Item](UInv_EquippedGridSlot* EquippedGridSlot)
	{
		return EquippedGridSlot->GetInventoryItem() == Item;
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

void UInv_SpatialInventory::ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot)
{
	if (!IsValid(EquippedGridSlot))
		return;
	EquippedGridSlot->SetEquippedSlotItem(nullptr);
	EquippedGridSlot->SetInventoryItem(nullptr);
}

void UInv_SpatialInventory::RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem)
{
	if (!IsValid(EquippedSlotItem))
		return;
	if (EquippedSlotItem->OnEquippedGridSlotItemClicked.IsAlreadyBound(this, &UInv_SpatialInventory::EquippedSlottedItemClicked))
	{
		EquippedSlotItem->OnEquippedGridSlotItemClicked.RemoveDynamic(this, &UInv_SpatialInventory::EquippedSlottedItemClicked);
	}
	EquippedSlotItem->RemoveFromParent();
}

void UInv_SpatialInventory::MakeEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem,
	UInv_EquippedGridSlot* EquippedGridSlot, UInv_InventoryItem* Item)
{
	if (!IsValid(EquippedGridSlot))
		return;
	UInv_EquippedSlotItem* SlottedItem = EquippedGridSlot->OnItemEquipped(Item, EquippedSlotItem->GetEquipmentTag(), UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize());
	if (IsValid(SlottedItem))
		SlottedItem->OnEquippedGridSlotItemClicked.AddDynamic(this, &UInv_SpatialInventory::EquippedSlottedItemClicked);
	EquippedGridSlot->SetEquippedSlotItem(SlottedItem);
}

void UInv_SpatialInventory::BroadcastSlotClickedDelegates(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip)
{
	if (!InventoryComponent.Get())
		return;

	// 只发 Server RPC，由服务器 Multicast 统一广播 OnItemEquipped/OnItemUnequipped，
	// 本地不再广播，否则换装回调（力量增加/降低）会执行两次
	InventoryComponent->Server_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

UInv_InventoryItem* UInv_SpatialInventory::FindNoFillItemLike(const FInv_ItemManifest& Source, TSet<UInv_InventoryItem*> Items) const
{
	return InventoryComponent->FindNoFillItem(Source, Items);
}

void UInv_SpatialInventory::InventoryBindEvent() const
{
	Grid_All->BindInventoryEvent(InventoryComponent.Get());
}

void UInv_SpatialInventory::ClearHoverItem() const
{
	Grid_All->SetHoverItem(nullptr);
	Grid_Other->SetHoverItem(nullptr);
}
