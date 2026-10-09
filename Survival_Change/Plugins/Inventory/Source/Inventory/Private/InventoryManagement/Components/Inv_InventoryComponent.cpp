#include "InventoryManagement/Components/Inv_InventoryComponent.h"

#include "Items/Components/Inv_ItemComponent.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Player/Inv_PlayerController.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"
#include "Widget/Utils/Inv_WidgetUtils.h"

UInv_InventoryComponent::UInv_InventoryComponent() : InventoryList(this)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	bReplicateUsingRegisteredSubObjectList = true;
	bInventoryMenuOpen = false;
}

void UInv_InventoryComponent::MoveItemOnServer(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result,
	int32 DropIndex)
{
	Server_MoveItem(SourceItem, Result, DropIndex);
}

void UInv_InventoryComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInv_InventoryComponent, InventoryList);
}

void UInv_InventoryComponent::TryAddItem(UInv_ItemComponent* ItemComponent)
{
	FInv_SlotAvailabilityResult Result = InventoryMenu->HasRoomForItem(ItemComponent);
	
	
	if (Result.TotalRoomToFill == 0)
	{
		NoRoomInInventory.Broadcast();
		return;
	}
	
	for (auto& AvailableItem : Result.SlotAvailabilities)
	{
		Server_AddItem(ItemComponent, AvailableItem.AmountToFill, AvailableItem.Index, AvailableItem.Item.Get(), AvailableItem.bItemAtIndex);
	}
	
	Server_DestroyActor(ItemComponent, Result.Remainder);
}

void UInv_InventoryComponent::Multicast_RemoveSlottedItem_Implementation(UInv_InventoryItem* SourceItem)
{
	if (!IsValid(SourceItem))
		return;
	OnRemoveSlottedItem.Broadcast(SourceItem, SourceItem->GetGridIndex());
}

void UInv_InventoryComponent::Server_RemoveSlottedItem_Implementation(UInv_InventoryItem* SourceItem)
{
	Multicast_RemoveSlottedItem(SourceItem);
}

void UInv_InventoryComponent::ChangeItemIndex(UInv_InventoryItem* Item, int32 Index)
{
	Item->SetGridIndex(Index);
	InventoryList.ChangeEntryIndex(Item, Index);
	Item->SetInventoryComponent(this);
	OnItemChanged.Broadcast(Item);
}

void UInv_InventoryComponent::Server_AddInventoryItem_Implementation(const int32 GridIndex, UInv_InventoryItem* Item, int32 StackCount, bool IsInGridSlots)
{
	if (IsInGridSlots)
	{
		InventoryList.ChangeEntryByIndex(Item, StackCount);
		Server_RemoveSlottedItem(Item);
		Item->SetInventoryComponent(this);
		OnItemChanged.Broadcast(Item);
		return;
	}
	UInv_InventoryItem* NewItem =  InventoryList.AddEntry(Item, GridIndex);
	NewItem->SetInventoryComponent(this);
	OnItemAdded.Broadcast(NewItem);
}

void UInv_InventoryComponent::Server_MoveItem_Implementation(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex)
{
	if (!IsValid(SourceItem))
		return;
	const FInv_StackableFragment* SourceStackable = SourceItem->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	const int32 DragStackCount = SourceStackable ? SourceStackable->GetStackCount() : 1;
	Server_RemoveSlottedItem(SourceItem);
	SourceItem->SetInventoryComponent(this);
	if (Result.bHasSpace)
	{
		SourceItem->SetGridIndex(DropIndex);
		InventoryList.ChangeEntryIndex(SourceItem, SourceItem->GetGridIndex());
		OnItemChanged.Broadcast(SourceItem);
	}
	else if (Result.bAllowItemDown)
	{
		if (Result.ValidItems.Num() == 1 && FInv_ItemManifest::AreSameItem(Result.ValidItems[0]->GetItemManifest(), SourceItem->GetItemManifest()))
		{
			UInv_InventoryItem* Item = Result.ValidItems[0].Get();
			Server_RemoveSlottedItem(Item);
			Item->SetInventoryComponent(this);
			if (const FInv_StackableFragment* StackableFragment = Result.ValidItems[0]->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>())
			{
				const int32 DropItemCount = StackableFragment->GetStackCount();
				const int32 MaxStackCount = StackableFragment->GetMaxStackCount();
				const int32 RoomInDropItem = MaxStackCount - DropItemCount;
				if (RoomInDropItem == 0)
				{
					ChangeItemIndex(SourceItem, Item->GetGridIndex());
					ChangeItemIndex(Item, Item->GetOldGridIndex());
				}
				else
				{
					const int32 DropCount = FMath::Min(DragStackCount, RoomInDropItem);
					const int32 RemainCount = DragStackCount - DropCount;
					InventoryList.ChangeEntryByIndex(Item, DropCount + DropItemCount);
					OnItemChanged.Broadcast(Item);
					if (RemainCount > 0)
					{
						InventoryList.ChangeEntryByIndex(SourceItem, RemainCount);
						OnItemChanged.Broadcast(SourceItem);
					}
					else
					{
						Server_RemoveItem(SourceItem);
					}
				}
			}
			else
			{
				ChangeItemIndex(SourceItem, Item->GetGridIndex());
				ChangeItemIndex(Item, Item->GetOldGridIndex());
			}
		}
		else
		{
			const int Num = Result.ValidItems.Num();
			for (int i = 0; i < Num; i++)
			{
				UInv_InventoryItem* Item = Result.ValidItems[i].Get();
				Server_RemoveSlottedItem(Item);
				Item->SetInventoryComponent(this);
				ChangeItemIndex(Item, Result.ItemMoveIndices[i]);
			}
			ChangeItemIndex(SourceItem, DropIndex);
		}
	}
	else
	{
		InventoryList.ChangeEntryByIndex(SourceItem, DragStackCount);
		OnItemChanged.Broadcast(SourceItem);
	}
}

void UInv_InventoryComponent::Server_DestroyActor_Implementation(UInv_ItemComponent* ItemComponent, int32 Remainder)
{
	if (Remainder == 0)
	{
		ItemComponent->PickedUp();
	}
	else if (FInv_StackableFragment* StackableFragment = ItemComponent->GetItemManifest().GetFragmentOfTypeMutable<FInv_StackableFragment>())
	{
		StackableFragment->SetStackCount(Remainder);
	}
}

void UInv_InventoryComponent::Server_AddItem_Implementation(UInv_ItemComponent* ItemComponent, const int32 StackCount, const int32 GridIndex, UInv_InventoryItem* Item, const bool IsInGridSlots)
{
	if (IsInGridSlots)
	{
		if (!IsValid(Item))
			return;
		InventoryList.ChangeEntry(Item, StackCount);
		Server_RemoveSlottedItem(Item);
		Item->SetInventoryComponent(this);
		OnItemChanged.Broadcast(Item);
	}
	else
	{
		if (!IsValid(ItemComponent))
			return;
		UInv_InventoryItem* NewItem = InventoryList.AddEntry(ItemComponent, StackCount, GridIndex);
		if (!IsValid(NewItem))
			return;
		NewItem->SetInventoryComponent(this);
		OnItemAdded.Broadcast(NewItem);
	}
}

void UInv_InventoryComponent::Server_DropItem_Implementation(UInv_InventoryItem* Item)
{
	SpawnDroppedItem(Item);
	InventoryList.RemoveEntry(Item);
	OnItemRemoved.Broadcast(Item);
}

UInv_ItemComponent* UInv_InventoryComponent::SpawnDroppedItem(UInv_InventoryItem* Item) const
{
	if (!IsValid(Item))
		return nullptr;

	//不依赖缓存的 OwningController（专用服务器上菜单不创建，它可能为空），直接从 Owner 取
	const APawn* OwningPawn = nullptr;
	if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
		OwningPawn = PC->GetPawn();
	if (!IsValid(OwningPawn) && OwningController.IsValid())
		OwningPawn = OwningController->GetPawn();
	if (!IsValid(OwningPawn))
	{
		return nullptr;
	}

	FVector RotateForward = OwningPawn->GetActorForwardVector();
	RotateForward = RotateForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotateForward;
	SpawnLocation.Z += RelativeSpawnElevation;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	
	FInv_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	return ItemManifest.SpawnPickUpActor(this, SpawnLocation, SpawnRotation);
}

UInv_InventoryItem* UInv_InventoryComponent::FindNoFillItem(const FInv_ItemManifest& Source, const TSet<UInv_InventoryItem*> Items) const
{
	return InventoryList.FindNoFillItemLike(Source, Items);
}

void UInv_InventoryComponent::Server_ConsumeItem_Implementation(UInv_InventoryItem* Item, int32 GridIndex)
{
	if (FInv_ConsumeFragment* ConsumeFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_ConsumeFragment>())
	{
		ConsumeFragment->OnConsume(OwningController.Get());
	}
}

void UInv_InventoryComponent::Server_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip)
{
	Multicast_EquipSlotClicked(ItemToEquip, ItemToUnequip);
}

void UInv_InventoryComponent::Multicast_EquipSlotClicked_Implementation(UInv_InventoryItem* ItemToEquip,
	UInv_InventoryItem* ItemToUnequip)
{
	OnItemEquipped.Broadcast(ItemToEquip);
	OnItemUnequipped.Broadcast(ItemToUnequip);
}

void UInv_InventoryComponent::Server_RemoveItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	RemoveRepSubObj(Item);
	InventoryList.RemoveEntry(Item);
	OnItemRemoved.Broadcast(Item);
}

void UInv_InventoryComponent::ToggleInventoryMenu()
{
	if (bInventoryMenuOpen)
	{
		CloseInventoryMenu();
	}
	else
	{
		OpenInventoryMenu();
	}
}

void UInv_InventoryComponent::AddRepSubObj(UObject* SubObj)
{
	if (!IsValid(SubObj))
		return;
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(SubObj);
	}
}

void UInv_InventoryComponent::RemoveRepSubObj(UObject* SubObj)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObj))
	{
		RemoveReplicatedSubObject(SubObj);
	}
}

void UInv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
}

void UInv_InventoryComponent::ConstructInventory()
{
	OwningController = GetWorld()->GetFirstPlayerController();
	if (!OwningController.IsValid() || !OwningController->IsLocalController())
		return;
	if (!IsValid(InventoryMenuClass))
		return;
	InventoryMenu = CreateWidget<UInv_InventoryBase>(OwningController.Get(), InventoryMenuClass);
	if (!IsValid(InventoryMenu))
		return;
	InventoryMenu->AddToViewport();
	InventoryMenu->SetInventoryComponent(this);
	CloseInventoryMenu();
}

void UInv_InventoryComponent::OpenInventoryMenu()
{
	if (!IsValid(InventoryMenu))
		return;
	InventoryMenu->SetVisibility(ESlateVisibility::Visible);
	bInventoryMenuOpen = true;
	if (!OwningController.IsValid())
		return;
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(InventoryMenu->TakeWidget());
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);
	OwningController->FindComponentByClass<UInv_FindItemComponent>()->SetInventoryGrid(InventoryMenu->GetOtherGrid());
	if (Cast<AInv_PlayerController>(GetOwner()))
	{
		InventoryMenu->GetInventoryGrid()->SetHoverSourceMenu(InventoryMenu);
		InventoryMenu->GetOtherGrid()->SetHoverSourceMenu(InventoryMenu);
		InventoryMenu->GetOtherGrid()->BindInventoryEvent(OwningController->FindComponentByClass<UInv_FindItemComponent>());
		InventoryMenu->GetOtherGrid()->InitInventory();
	}
}

void UInv_InventoryComponent::CloseInventoryMenu()
{
	if (!IsValid(InventoryMenu))
		return;
	InventoryMenu->SetVisibility(ESlateVisibility::Collapsed);
	bInventoryMenuOpen = false;
	if (!OwningController.IsValid())
		return;
	FInputModeGameOnly InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(false);
	InventoryMenu->GetOtherGrid()->UnBindInventoryEvent();
}

