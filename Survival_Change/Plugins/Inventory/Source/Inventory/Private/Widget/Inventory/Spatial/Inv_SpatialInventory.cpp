#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"
#include "Components/WidgetSwitcher.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Components/Button.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Widget/ItemDescription/Inv_ItemDescription.h"
#include "Inventory.h"
#include "Blueprint/DragDropOperation.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Building/Inv_Building.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Inv_ItemTags.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Blueprint/WidgetTree.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/GridSlot/Inv_EquippedGridSlot.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/SlottedItems/Inv_EquippedSlotItem.h"
#include "Widget/OpenInventory/Inv_OpenInventoryWidget.h"
#include "InventoryManagement/Grid/Inv_GridLayout.h"

void UInv_SpatialInventory::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	SetIsFocusable(true);
	/*Button_Equippable->OnClicked.AddDynamic(this, &ThisClass::ShowAll);
	Button_Consumable->OnClicked.AddDynamic(this, &ThisClass::ShowConsumable);
	Button_Craftable->OnClicked.AddDynamic(this, &ThisClass::ShowCraftable);*/

	Grid_All->SetOwningCanvasPanel(CanvasPanel);
	Grid_All->SetHoverSourceMenu(this);
	for (UInv_InventoryGrid* Grid : {BackPackInventory.Get(), ChestRigInventory.Get()})
	{
		Grid->SetOwningCanvasPanel(CanvasPanel);
		Grid->SetHoverSourceMenu(this);
		Grid->SetVisibility(ESlateVisibility::Collapsed);
	}
	VerticalBox->ClearChildren();
	
	Survival_ActorEquipment->WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UInv_EquippedGridSlot* EquippedGridSlot = Cast<UInv_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			Equipments.Add(EquippedGridSlot);
			EquippedGridSlot->SetInventoryGrid(Grid_All);
			EquippedGridSlot->SetNormal();
		}
	});
	
	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		UInv_EquippedGridSlot* EquippedGridSlot = Cast<UInv_EquippedGridSlot>(Widget);
		if (IsValid(EquippedGridSlot))
		{
			Equipments.Add(EquippedGridSlot);
			EquippedGridSlot->SetInventoryGrid(Grid_All);
			EquippedGridSlot->SetNormal();
		}
	});
}

void UInv_SpatialInventory::NativeDestruct()
{
	BackPackInventory->UnBindInventoryEvent();
	ChestRigInventory->UnBindInventoryEvent();
	SetOpenInventory(nullptr);
	Super::NativeDestruct();
}

void UInv_SpatialInventory::EquippedItem(UInv_EquippedGridSlot* EquippedGridSlot,
	const FGameplayTag& EquipmentTag) const
{
	UInv_HoverItem* HoverItem = GetHoverItem();
	if (!IsValid(HoverItem) || !IsValid(HoverItem->GetInventoryItem()))
		return;

	// 在 ClearHoverItem 之前缓存物品指针
	UInv_InventoryItem* ItemToEquip = HoverItem->GetInventoryItem();

	const float TileSize = UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UInv_EquippedSlotItem* EquippedSlotItem = EquippedGridSlot->OnItemEquipped(ItemToEquip, TileSize);
}

// 显示装备槽物品；装备背包或弹挂时，同时绑定并显示其内部库存网格。
void UInv_SpatialInventory::EquipItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	UInv_EquippedGridSlot* EquippedGridSlot = FindSlotWithEquippedType(Item->GetItemManifest().GetItemType());
	const float TileSize = UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize();
	UInv_EquippedSlotItem* EquippedSlotItem = EquippedGridSlot->OnItemEquipped(Item, TileSize);
	EquippedSlotItem->SetInventoryGrid(Grid_All);
	if (Item->GridAsset)
	{
		UInv_InventoryGrid* Grid = Item->GetItemManifest().GetItemType().MatchesTag(GameItems::Equipment::ChestRigs::ChestRig)
			? ChestRigInventory.Get() : BackPackInventory.Get();
		Grid->BindInventoryEvent(Item->GetInventoryComponent(), Item);
		// 保留蓝图默认高度值，只关闭固定高度，让内部网格按资产行数和格子尺寸撑开。
		USizeBox* GridBox = Grid == ChestRigInventory ? SizeBox_ChestRig.Get() : SizeBox_BackPack.Get();
		GridBox->ClearHeightOverride();
		Grid->SetVisibility(ESlateVisibility::Visible);
	}
}

// 移除装备槽显示；容器装备只解绑自己的网格，内部物品数据仍保存在容器物品上。
void UInv_SpatialInventory::UnEquipItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	for (UInv_InventoryGrid* Grid : {BackPackInventory.Get(), ChestRigInventory.Get()})
	{
		if (Grid->ContainerItem == Item)
		{
			Grid->UnBindInventoryEvent();
			Grid->SetVisibility(ESlateVisibility::Collapsed);
			// ClearHeightOverride 不会清除原值，卸装时重新启用蓝图配置的默认高度。
			USizeBox* GridBox = Grid == ChestRigInventory ? SizeBox_ChestRig.Get() : SizeBox_BackPack.Get();
			GridBox->SetHeightOverride(GridBox->GetHeightOverride());
		}
	}
	UInv_EquippedGridSlot* GridSlot = FindSlotWithEquippedItem(Item);
	if (!IsValid(GridSlot))
		return;
	RemoveEquippedSlottedItem(GridSlot->GetEquippedSlotItem());
	ClearSlotItem(GridSlot);
}

FReply UInv_SpatialInventory::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	return FReply::Handled();
}

bool UInv_SpatialInventory::NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent,
	UDragDropOperation* InOperation)
{
	for (UInv_InventoryGrid* Grid : {BackPackInventory.Get(), ChestRigInventory.Get()})
	{
		if (Grid->ContainerItem && Grid->GetCachedGeometry().IsUnderLocation(InDragDropEvent.GetScreenSpacePosition()))
		{
			ClearHoverItem();
			return true;
		}
	}
	UInv_HoverItem* HoverItem = Cast<UInv_HoverItem>(InOperation->Payload);
	APlayerController* PlayerController = GetOwningPlayer();
	AInv_PlayerController* Controller = Cast<AInv_PlayerController>(PlayerController);
	UInv_RequestComponent* RequestComp = Controller->FindComponentByClass<UInv_RequestComponent>();
	
	if (Cast<UInv_FindItemComponent>(HoverItem->GetInventoryItem()->GetInventoryComponent()))
	{
		// 附近物品本就位于世界中，拖出界面只取消拖拽，不能再次丢弃。
		ClearHoverItem();
		return true;
	}
	
	RequestComp->DropItem(HoverItem->GetInventoryItem());
	ClearHoverItem();
	return true;
}

FReply UInv_SpatialInventory::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		InventoryComponent->ToggleInventoryMenu();
		return FReply::Handled();
	}
	if (OpenInventory && InKeyEvent.GetKey() == EKeys::B)
		return FReply::Handled();
	
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

FInv_SlotAvailabilityResult UInv_SpatialInventory::HasRoomForItem(UInv_InventoryItem* InventoryItem) const
{
	return Grid_All->HasRoomForItem(InventoryItem);
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
	return IsValid(GetHoverItem());
}

UInv_HoverItem* UInv_SpatialInventory::GetHoverItem() const
{
	if (Grid_All->HasHoveredItem())
		return Grid_All->GetHoverItem();
	if (BackPackInventory->HasHoveredItem())
		return BackPackInventory->GetHoverItem();
	if (ChestRigInventory->HasHoveredItem())
		return ChestRigInventory->GetHoverItem();
	if (OpenInventory && OpenInventory->HasHoveredItem())
		return OpenInventory->GetHoverItem();
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

void UInv_SpatialInventory::ShowAll() const
{
	//SetActiveGrid(Grid_All, Button_Equippable);
}

void UInv_SpatialInventory::ShowConsumable() const
{
}

void UInv_SpatialInventory::ShowCraftable() const
{
}

void UInv_SpatialInventory::DisableButton(UButton* Button) const
{
	Button->SetIsEnabled(false);
}

void UInv_SpatialInventory::SetActiveGrid(UInv_InventoryGrid* Grid, UButton* Button) const
{
	/*if (ActiveGrid.IsValid())
	{
		ActiveGrid->OnHide();
	}
	ActiveGrid = Grid;
	if (ActiveGrid.IsValid())
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
	return IsValid(Item) && Item->GetItemManifest().GetItemType().MatchesTag(Tag);
}

UInv_EquippedGridSlot* UInv_SpatialInventory::FindSlotWithEquippedItem(UInv_InventoryItem* Item) const
{
	auto* FoundEquippedGridSlot = Equipments.FindByPredicate([Item](UInv_EquippedGridSlot* EquippedGridSlot)
	{
		return EquippedGridSlot->GetInventoryItem() == Item;
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

UInv_EquippedGridSlot* UInv_SpatialInventory::FindSlotWithEquippedType(const FGameplayTag& Tag) const
{
	auto* FoundEquippedGridSlot = Equipments.FindByPredicate([&Tag](UInv_EquippedGridSlot* EquippedGridSlot)
	{
		return Tag.MatchesTag(EquippedGridSlot->GetEquippedTag());
	});
	return FoundEquippedGridSlot ? *FoundEquippedGridSlot : nullptr;
}

void UInv_SpatialInventory::ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot) const
{
	if (!IsValid(EquippedGridSlot))
		return;
	EquippedGridSlot->SetEquippedSlotItem(nullptr);
	EquippedGridSlot->SetInventoryItem(nullptr);
}

void UInv_SpatialInventory::RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem) const
{
	if (!IsValid(EquippedSlotItem))
		return;
	EquippedSlotItem->RemoveFromParent();
}

void UInv_SpatialInventory::MakeEquippedSlottedItem(UInv_EquippedGridSlot* EquippedGridSlot, UInv_InventoryItem* Item) const
{
	if (!IsValid(EquippedGridSlot))
		return;
	UInv_EquippedSlotItem* SlottedItem = EquippedGridSlot->OnItemEquipped(Item, UInv_InventoryStatics::GetInventoryWidget(GetOwningPlayer())->GetTileSize());
}

void UInv_SpatialInventory::BroadcastSlotClickedDelegates(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip, int32 ItemDropIndex,
	const UInv_InventoryGrid* SourceGrid, const UInv_InventoryGrid* TargetGrid, int32 ItemDropSubGridIndex) const
{
	if (!InventoryComponent.Get())
		return;
	
	//InventoryComponent->Server_EquipItem(ItemToEquip, ItemToUnequip);
	UInv_HoverItem* HoverItem = GetHoverItem();
	const bool bUnequip = HoverItem->GetPreviousGridIndex() == INDEX_NONE;
	const int32 SourceIndex = bUnequip
		? (ItemToEquip ? ItemToEquip->GetGridIndex() : INDEX_NONE) : HoverItem->GetPreviousGridIndex();
	const FGameplayTag EquipmentType = bUnequip
		? HoverItem->EquipmentType : FindSlotWithEquippedItem(ItemToEquip)->GetEquippedTag();
	GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>()->Server_EquipItem(ItemToEquip, ItemToUnequip,
		ItemDropIndex, TargetGrid->GetInventoryComponent(), SourceGrid->GetInventoryComponent(),
		EquipmentType, SourceIndex, SourceGrid->ContainerItem,
		FMath::Max(0, SourceGrid->SubGridIndex), ItemDropSubGridIndex == INDEX_NONE ? FMath::Max(0, SourceGrid->SubGridIndex) : ItemDropSubGridIndex);
}

UInv_InventoryItem* UInv_SpatialInventory::FindNoFillItemLike(const FInv_ItemManifest& Source, const TSet<UInv_InventoryItem*>& Items) const
{
	return InventoryComponent->FindNoFillItem(Source, Items);
}

void UInv_SpatialInventory::InventoryBindEvent() const
{
	Grid_All->BindInventoryEvent(InventoryComponent.Get());
	InventoryComponent->OnItemEquipped.AddDynamic(this, &UInv_SpatialInventory::EquipItem);
	InventoryComponent->OnItemUnequipped.AddDynamic(this, &UInv_SpatialInventory::UnEquipItem);
}

// 替换右侧打开库存的界面；使用目标组件配置的 UI 类生成网格，名称与布局由该界面初始化。
void UInv_SpatialInventory::SetOpenInventory(UInv_InventoryComponent* InInventoryComponent)
{
	FinishDragPrediction();
	if (OpenInventory)
		OpenInventory->UnBindInventoryEvent();
	OpenInventory = nullptr;
	Grid_Other = nullptr;
	VerticalBox->ClearChildren();
	if (!InInventoryComponent)
		return;
	UInv_OpenInventoryWidget* InventoryWidget = CreateWidget<UInv_OpenInventoryWidget>(GetOwningPlayer(), InInventoryComponent->OpenInventoryClass);
	VerticalBox->AddChildToVerticalBox(InventoryWidget);
	OpenInventory = InventoryWidget->InitInventory(InInventoryComponent, this, CanvasPanel);
	if (Cast<UInv_FindItemComponent>(InInventoryComponent))
		Grid_Other = OpenInventory;
}

void UInv_SpatialInventory::ClearHoverItem() const
{
	Grid_All->SetHoverItem(nullptr);
	BackPackInventory->SetHoverItem(nullptr);
	ChestRigInventory->SetHoverItem(nullptr);
	if (OpenInventory)
		OpenInventory->SetHoverItem(nullptr);
}

void UInv_SpatialInventory::RemoveSlotItem(UInv_SlottedItem* Item, UInv_InventoryGrid* TargetGrid)
{
	UInv_HoverItem* HoverItem = GetHoverItem();
	const int32 SourceIndex = HoverItem->GetPreviousGridIndex();
	UInv_InventoryItem* DraggedItem = HoverItem->GetInventoryItem();
	UInv_InventoryGrid* SourceGrid = HoverItem->GetGrid();
	if (SourceIndex == INDEX_NONE)
	{
		const FInv_SpaceQueryResult Result = TargetGrid->GetResult();
		UInv_InventoryItem* ItemToEquip = Result.ValidItems.IsEmpty() ? nullptr : Result.ValidItems[0].Get();
		BroadcastSlotClickedDelegates(ItemToEquip, DraggedItem, TargetGrid->GetItemDropIndex(), TargetGrid, SourceGrid);
		return;
	}
	UInv_InventoryItem* ItemToUnequip = Item ? Item->GetInventoryItem() : nullptr;
	int32 ItemDropIndex = SourceIndex;
	int32 ItemDropSubGridIndex = SourceGrid->SubGridIndex;
	if (ItemToUnequip)
	{
		if (!SourceGrid->GetInventoryComponent()->AddItem(ItemToUnequip, ItemDropIndex, SourceGrid, &ItemDropSubGridIndex))
		{
			FinishDragPrediction();
			return;
		}
		Item->RemoveFromParent();
	}
	BroadcastSlotClickedDelegates(DraggedItem, ItemToUnequip, ItemDropIndex, SourceGrid, TargetGrid, ItemDropSubGridIndex);
}

// 取消当前拖拽，按当前复制数据重建网格和装备 UI，清除本地预览产生的显示差异。
void UInv_SpatialInventory::FinishDragPrediction()
{
	UWidgetBlueprintLibrary::CancelDragDrop();
	// 重建装备 UI 前先恢复空槽高度，随后 EquipItem 会适配仍然穿着的容器网格。
	SizeBox_BackPack->SetHeightOverride(SizeBox_BackPack->GetHeightOverride());
	SizeBox_ChestRig->SetHeightOverride(SizeBox_ChestRig->GetHeightOverride());
	for (UInv_InventoryGrid* Grid : {BackPackInventory.Get(), ChestRigInventory.Get()})
	{
		Grid->UnBindInventoryEvent();
		Grid->SetVisibility(ESlateVisibility::Collapsed);
	}
	UInv_InventoryGrid* Grids[] = {Grid_All, OpenInventory};
	for (UInv_InventoryGrid* Grid : Grids)
	{
		if (!Grid)
			continue;
		Grid->ClearInventory();
		if (Grid->GetInventoryComponent())
		{
			Grid->InitInventory();
		}
	}
	for (UInv_EquippedGridSlot* EquipmentSlot : Equipments)
	{
		RemoveEquippedSlottedItem(EquipmentSlot->GetEquippedSlotItem());
		ClearSlotItem(EquipmentSlot);
		EquipmentSlot->SetNormal();
	}
	if (InventoryComponent.IsValid())
	{
		for (const FInv_InventoryEntry& Entry : InventoryComponent->GetInventoryList().GetAllItems())
		{
			if (Entry.GridIndex == INDEX_NONE)
			{
				EquipItem(Entry.GetItem());
			}
		}
	}
}

