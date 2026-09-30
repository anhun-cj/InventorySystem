#include "Items/Manifest/Inv_ItemManifest.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Widget/Composite/Inv_CompositeBase.h"

UInv_InventoryItem* FInv_ItemManifest::Manifest(UObject* NewOuter)
{
	UInv_InventoryItem * Item = NewObject<UInv_InventoryItem>(NewOuter, UInv_InventoryItem::StaticClass());
	Item->SetItemManifest(*this);
	
	for (auto& Fragment : Item->GetItemManifestMutable().GetAllFragments())
	{
		Fragment.GetMutable().Manifest();
	}
	ClearFragments();
	return Item;
}

void FInv_ItemManifest::AssimilateInventoryFragment(UInv_CompositeBase* Composite) const
{
	const auto& InvFragments = GetAllFragmentsOfType<FInv_InventoryItemFragment>();
	for (const auto* Fragment : InvFragments)
	{
		Composite->ApplyFunction([Fragment](UInv_CompositeBase* Widget)
		{
			Fragment->Assimilate(Widget);
		});
	}
}

void FInv_ItemManifest::SpawnPickUpActor(const UObject* WorldContextObject, const FVector& SpawnLocation,
                                         const FRotator& SpawnRotation) const
{
	if (!IsValid(WorldContextObject) || !IsValid(PickupActorClass))
		return;
	AActor* SpawnActor = WorldContextObject->GetWorld()->SpawnActor<AActor>(PickupActorClass, SpawnLocation, SpawnRotation);
	if (!IsValid(SpawnActor))
		return;
	UInv_ItemComponent* ItemComponent = SpawnActor->FindComponentByClass<UInv_ItemComponent>();
	check(ItemComponent);
	ItemComponent->InitItemManifest(*this);
}

void FInv_ItemManifest::ClearFragments()
{
	for (auto& Fragment : Fragments)
	{
		Fragment.Reset();
	}
	Fragments.Empty();
}
