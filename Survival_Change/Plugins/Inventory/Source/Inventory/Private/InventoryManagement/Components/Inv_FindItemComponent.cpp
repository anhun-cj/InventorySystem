#include "InventoryManagement/Components/Inv_FindItemComponent.h"

#include "Engine/OverlapResult.h"
#include "InventoryManagement/Grid/Inv_GridLayout.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Player/Inv_PlayerController.h"


UInv_FindItemComponent::UInv_FindItemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetComponentTickInterval(0.2);

	OnItemAdded.AddDynamic(this, &UInv_FindItemComponent::OnItemAdd);
	OnItemChanged.AddDynamic(this, &UInv_FindItemComponent::OnItemChange);
	OnItemRemoved.AddDynamic(this, &UInv_FindItemComponent::OnItemRemove);
}

void UInv_FindItemComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		ClearNoValidItemComponents();
	}

	AInv_PlayerController* PC = Cast<AInv_PlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController())
		return;

	if (!InventoryGrid.IsValid())
		return;

	UWorld* World = GetWorld();
	if (!World)
		return;
	
	APawn* Pawn = PC->GetPawn();
	if (!IsValid(Pawn))
		return;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PC);
	Params.AddIgnoredActor(Pawn);

	TArray<FOverlapResult> OverlapResults;
	const FCollisionShape QuerySphere = FCollisionShape::MakeSphere(DetectRadius);
	if (!World->OverlapMultiByChannel(OverlapResults, Pawn->GetActorLocation(), FQuat::Identity,
		ECC_GameTraceChannel1, QuerySphere, Params))
	{
		return;
	}

	//距离排序Item
	OverlapResults.Sort([Pawn](const FOverlapResult& A, const FOverlapResult& B)
	{
		const AActor* ActorA = A.GetActor();
		const AActor* ActorB = B.GetActor();
		const double DistA = ActorA
			? FVector::DistSquared(ActorA->GetActorLocation(), Pawn->GetActorLocation())
			: TNumericLimits<double>::Max();
		const double DistB = ActorB
			? FVector::DistSquared(ActorB->GetActorLocation(), Pawn->GetActorLocation())
			: TNumericLimits<double>::Max();
		return DistA < DistB;
	});

	for (const FOverlapResult& Result : OverlapResults)
	{
		AActor* HitActor = Result.GetActor();
		if (!IsValid(HitActor))
			continue;

		UInv_ItemComponent* ItemComponent = HitActor->FindComponentByClass<UInv_ItemComponent>();
		if (!IsValid(ItemComponent))
			continue;
		if (DetectedComponents.Contains(ItemComponent))
			continue;
		DetectedComponents.Add(ItemComponent);
		
		Server_RequestPickUp(ItemComponent);
	}
}

void UInv_FindItemComponent::SetInventoryGrid(UInv_InventoryGrid* Grid)
{
	InventoryGrid = Grid;
	if (!Grid)
	{
		DetectedComponents.Empty();
		Server_ClearItemComponents();
	}
}

void UInv_FindItemComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UInv_FindItemComponent::SplitNearbyItem(UInv_InventoryItem* Item, int32 Count)
{
	UInv_ItemComponent* SourcePickup = FindItemComponent(Item);
	if (!IsValid(SourcePickup) || !InventoryList.FindItem(Item) || Count <= 0)
		return;
	const FInv_StackableFragment* Stackable = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	if (!Stackable || Count >= Stackable->GetStackCount())
		return;
	const int32 OriginalCount = Stackable->GetStackCount();
	AActor* SourceActor = SourcePickup->GetOwner();
	if (!IsValid(SourceActor) || !GetWorld())
		return;

	//寻找拆分落点，空间不足时自动扩行。
	TArray<UInv_InventoryItem*> ExistingItems;
	for (const FInv_InventoryEntry& Entry : InventoryList.GetAllItems())
	{
		ExistingItems.Add(Entry.GetItem());
	}
	TArray<bool> Occupancy;
	const int32 RequiredRows = FInv_GridLayout::GetRequiredRows(ExistingItems, GridAsset->Grids[0]->Col, GridAsset->Grids[0]->Row);
	FInv_GridLayout::BuildOccupancy(ExistingItems, GridAsset->Grids[0]->Col, RequiredRows, Occupancy);
	FInv_GridLayout Layout;
	Layout.Setup(GridAsset->Grids[0]->Col, RequiredRows, Occupancy);
	const FInv_SlotAvailabilityResult Space = Layout.QueryWithGrowth(Item->GetItemManifest(), Count, false, {});
	if (Space.SlotAvailabilities.IsEmpty() || Space.SlotAvailabilities[0].AmountToFill < Count)
		return;
	int32 Index = Space.SlotAvailabilities[0].Index;
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	const APawn* NearbyPawn = Controller ? Controller->GetPawn() : nullptr;
	if (!IsValid(NearbyPawn))
		return;
	const FTransform SpawnTransform(SourceActor->GetActorRotation(), NearbyPawn->GetActorLocation(), SourceActor->GetActorScale3D());
	AActor* NewActor = GetWorld()->SpawnActor<AActor>(SourceActor->GetClass(), SpawnTransform, SpawnParams);
	if (!IsValid(NewActor))
		return;
	NewActor->SetActorTransform(SpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
	NewActor->SetActorLocation(GetDropSpawnLocation(NearbyPawn),
		false, nullptr, ETeleportType::TeleportPhysics);
	UInv_ItemComponent* NewPickup = NewActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(NewPickup))
	{
		NewActor->Destroy();
		return;
	}
	UInv_InventoryItem* NewItem = InventoryList.AddEntryOnSplit(Item, Index, Count);
	NewPickup->ItemDefinition = SourcePickup->ItemDefinition;
	NewPickup->InitItemManifest(NewItem->GetItemManifest());
	ItemComponents.Add(NewItem, NewPickup);
	Server_RemoveSlottedItem(Item);
	InventoryList.ChangeEntryByIndex(Item, OriginalCount - Count);
	OnItemChanged.Broadcast(Item);
	OnItemAdded.Broadcast(NewItem);
}

void UInv_FindItemComponent::OnItemChange(UInv_InventoryItem* Item)
{
	if (!GetOwner()->HasAuthority())
		return;
	UInv_ItemComponent* ItemComponent = FindItemComponent(Item);
	if (!IsValid(ItemComponent))
		return;
	FInv_StackableFragment* StackFragment = Item->GetItemManifestMutable().GetFragmentOfTypeMutable<FInv_StackableFragment>();
	UpdatePickupActor(ItemComponent, StackFragment ? StackFragment->GetStackCount() : 1);
}

void UInv_FindItemComponent::Server_RequestPickUp_Implementation(UInv_ItemComponent* ItemComponent)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;
	if (!IsValid(ItemComponent))
		return;
	
	const AActor* ItemActor = ItemComponent->GetOwner();
	if (!IsValid(ItemActor))
		return;

	for (const auto& Pair : ItemComponents)
	{
		if (Pair.Value.Get() == ItemComponent)
			return;
	}

	AddItemOnServer(ItemComponent);
}

void UInv_FindItemComponent::AddItemOnServer(UInv_ItemComponent* ItemComponent)
{
	if (!IsValid(ItemComponent))
		return;

	const FInv_ItemManifest& Manifest = ItemComponent->GetItemManifest();
	
	const TArray<FInv_InventoryEntry> Entries = InventoryList.GetAllItems();
	
	TArray<UInv_InventoryItem*> ExistingItems;
	ExistingItems.Reserve(Entries.Num());
	TMap<int32, TWeakObjectPtr<UInv_InventoryItem>> ItemLookup;
	for (const FInv_InventoryEntry& Entry : Entries)
	{
		UInv_InventoryItem* Item = Entry.GetItem();
		if (!IsValid(Item))
			continue;
		ExistingItems.Add(Item);
		if (Item->GetGridIndex() != INDEX_NONE)
		{
			ItemLookup.Add(Item->GetGridIndex(), Item);
		}
	}

	TArray<bool> Occupancy;
	const int32 OccupiedRows = FInv_GridLayout::GetRequiredRows(ExistingItems, GridAsset->Grids[0]->Col, GridAsset->Grids[0]->Row);
	FInv_GridLayout::BuildOccupancy(ExistingItems, GridAsset->Grids[0]->Col, OccupiedRows, Occupancy);

	FInv_GridLayout Layout;
	Layout.Setup(GridAsset->Grids[0]->Col, OccupiedRows, Occupancy);
	const FInv_SlotAvailabilityResult Result = Layout.QueryWithGrowth(Manifest, -1, false, ItemLookup);

	if (Result.TotalRoomToFill == 0)
		return;

	const FIntPoint Dimensions = FInv_GridLayout::GetItemDimensions(Manifest);

	for (const FInv_SlotAvailability& Slot : Result.SlotAvailabilities)
	{
		UInv_InventoryItem* NewItem = GetInventoryListMutable().AddEntry(
			ItemComponent, Slot.AmountToFill, Slot.Index);
		if (!IsValid(NewItem))
			continue;

		NewItem->SetInventoryComponent(this);
		ItemComponents.Add(NewItem, ItemComponent);
		OnItemAdded.Broadcast(NewItem);
		
		Layout.OccupyEntry(Slot.Index, Dimensions);
	}
}

void UInv_FindItemComponent::OnItemAdd(UInv_InventoryItem* Item)
{
	if (!GetOwner()->HasAuthority())
		return;
	if (!IsValid(Item) || ItemComponents.Contains(Item) || Item->GetInventoryComponent() == Item->GetOldInventoryComponent())
		return;
	UInv_ItemComponent* ItemComponent = SpawnDroppedItem(Item);
	if (!IsValid(ItemComponent))
		return;
	
	ItemComponent->InitItemManifest(Item->GetItemManifest());
	ItemComponents.Add(Item, ItemComponent);
}

void UInv_FindItemComponent::OnItemRemove(UInv_InventoryItem* Item)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
		return;
	if (!IsValid(Item))
		return;
	UInv_ItemComponent* ItemComponent = FindItemComponent(Item);
	if (IsValid(ItemComponent))
	{
		if (Item->GridAsset)
		{
			UInv_InventoryComponent* GroundInventory = ItemComponent->GetOwner()->FindComponentByClass<UInv_InventoryComponent>();
			FInv_InventoryFastArray& GroundList = GroundInventory->GetInventoryListMutable();
			const TArray<FInv_InventoryEntry> Contents = GroundList.GetAllItems();
			for (const FInv_InventoryEntry& Entry : Contents)
				GroundInventory->RemoveRepSubObj(Entry.GetItem());
			GroundList.ClearAllItems();
			Item->InventoryList.Entries = Contents;
			for (FInv_InventoryEntry& Entry : Item->InventoryList.Entries)
			{
				Entry.GetItem()->ParentContainerItem = Item;
				Item->InventoryList.MarkItemDirty(Entry);
				GroundInventory->OnItemRemoved.Broadcast(Entry.GetItem());
			}
		}
		const FInv_StackableFragment* StackableFragment = ItemComponent->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
		const FInv_StackableFragment* StackableFragment1 = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
		const int32 Count = (StackableFragment && StackableFragment1)
			? FMath::Max(0, StackableFragment->GetStackCount() - StackableFragment1->GetStackCount())
			: 0;
		UpdatePickupActor(ItemComponent, Count);
	}
	ItemComponents.Remove(Item);
}

void UInv_FindItemComponent::ClearNoValidItemComponents()
{
	TArray<TWeakObjectPtr<UInv_InventoryItem>> KeysToRemove;
	for (const auto& Pair : ItemComponents)
	{
		if (!IsValid(Pair.Value.Get()))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (const auto& Key : KeysToRemove)
	{
		if (UInv_InventoryItem* Item = Key.Get())
		{
			Server_RemoveItem(Item);
		}
		ItemComponents.Remove(Key);
	}
}

void UInv_FindItemComponent::Server_ClearItemComponents_Implementation()
{
	ItemComponents.Empty();
	InventoryList.ClearAllItems();
}

UInv_ItemComponent* UInv_FindItemComponent::FindItemComponent(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return nullptr;

	if (const TWeakObjectPtr<UInv_ItemComponent>* Found = ItemComponents.Find(Item))
	{
		UInv_ItemComponent* ItemComponent = Found->Get();
		if (IsValid(ItemComponent))
			return ItemComponent;
	}
	return nullptr;
}
