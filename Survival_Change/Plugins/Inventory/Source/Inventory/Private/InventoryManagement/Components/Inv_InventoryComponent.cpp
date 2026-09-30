#include "InventoryManagement/Components/Inv_InventoryComponent.h"

#include "Items/Components/Inv_ItemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_ItemFragment.h"
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
	OnItemChanged.Broadcast(Item);
}

void UInv_InventoryComponent::Server_MoveItem_Implementation(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex)
{
	const FInv_StackableFragment* SourceStackable = SourceItem->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	const int32 DragStackCount = SourceStackable ? SourceStackable->GetStackCount() : 1;
	Server_RemoveSlottedItem(SourceItem);
	if (Result.bHasSpace)
	{
		SourceItem->SetGridIndex(DropIndex);
		InventoryList.ChangeEntryIndex(SourceItem, SourceItem->GetGridIndex());
		OnItemChanged.Broadcast(SourceItem);
	}
	else if (Result.bAllowItemDown)
	{
		if (Result.ValidItems.Num() == 1 && Result.ValidItems[0]->GetItemManifest().GetItemType().MatchesTagExact(SourceItem->GetItemManifest().GetItemType()))
		{
			UInv_InventoryItem* Item = Result.ValidItems[0].Get();
			Server_RemoveSlottedItem(Item);
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
						InventoryList.RemoveEntry(SourceItem);
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
		InventoryList.ChangeEntry(Item, StackCount);
		Server_RemoveSlottedItem(Item);
		OnItemChanged.Broadcast(Item);
	}
	else
	{
		UInv_InventoryItem* NewItem =  InventoryList.AddEntry(ItemComponent, StackCount, GridIndex);
		OnItemAdded.Broadcast(NewItem);
	}
}

void UInv_InventoryComponent::Server_AddNewItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount)
{
	
	if (GetOwner()->GetNetMode() == NM_ListenServer || GetOwner()->GetNetMode() == NM_Standalone)
	{
		
	}
	ItemComponent->PickedUp();
}

void UInv_InventoryComponent::Server_AddStacksToItem_Implementation(UInv_ItemComponent* ItemComponent, int32 StackCount,
	int32 Remainder)
{
	
}

void UInv_InventoryComponent::Server_DropItem_Implementation(UInv_InventoryItem* Item)
{
	SpawnDroppedItem(Item);
	InventoryList.RemoveEntry(Item);
	OnItemRemoved.Broadcast(Item);
}

void UInv_InventoryComponent::SpawnDroppedItem(UInv_InventoryItem* Item) const
{
	const APawn* OwningPawn = OwningController->GetPawn();
	FVector RotateForward = OwningPawn->GetActorForwardVector();
	RotateForward = RotateForward.RotateAngleAxis(FMath::FRandRange(DropSpawnAngleMin, DropSpawnAngleMax), FVector::UpVector);
	FVector SpawnLocation = OwningPawn->GetActorLocation() + RotateForward;
	SpawnLocation.Z += RelativeSpawnElevation;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	
	FInv_ItemManifest& ItemManifest = Item->GetItemManifestMutable();
	ItemManifest.SpawnPickUpActor(this, SpawnLocation, SpawnRotation);
}

UInv_InventoryItem* UInv_InventoryComponent::FindNoFillItemByType(const FGameplayTag& Tag, const TSet<UInv_InventoryItem*> Items) const
{
	return InventoryList.FindNoFillItemByType(Tag, Items);
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
	OnInventoryMenuToggled.Broadcast(bInventoryMenuOpen);
}

void UInv_InventoryComponent::AddRepSubObj(UObject* SubObj)
{
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && IsValid(SubObj))
	{
		AddReplicatedSubObject(SubObj);
	}
}

void UInv_InventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	ConstructInventory();
}

void UInv_InventoryComponent::ConstructInventory()
{
	OwningController = Cast<APlayerController>(GetOwner());
	checkf(OwningController.IsValid(), TEXT("库存组件应该在玩家控制器上"));
	if (!OwningController->IsLocalController())
		return;
	InventoryMenu = CreateWidget<UInv_InventoryBase>(OwningController.Get(), InventoryMenuClass);
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
	FInputModeGameAndUI InputMode;
	OwningController->SetInputMode(InputMode);
	OwningController->SetShowMouseCursor(true);
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
}

