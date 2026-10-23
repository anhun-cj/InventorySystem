#include "EquipmentManager/Components/Inv_EquipmentComponent.h"

#include "EquipmentManager/EquipActor/Inv_EquipActor.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_ItemFragment.h"

void UInv_EquipmentComponent::SetOwningSkeletalMeshComponent(USkeletalMeshComponent* SkeletalMesh)
{
	SkeletalMeshComponent = SkeletalMesh;
}

void UInv_EquipmentComponent::InitializeOwner(APlayerController* PC)
{
	if (IsValid(PC))
	{
		PlayerController = PC;
	}
	InitInventoryComponent();
}

void UInv_EquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	InitPlayerController();
}

void UInv_EquipmentComponent::InitPlayerController()
{
	PlayerController = Cast<APlayerController>(GetOwner());
	if (PlayerController.IsValid())
	{
		ACharacter* Character = Cast<ACharacter>(PlayerController->GetPawn());
		if (IsValid(Character))
		{
			OnPossessesPawnChange(nullptr, Character);
		}
		else
		{
			PlayerController->OnPossessedPawnChanged.AddDynamic(this, &UInv_EquipmentComponent::OnPossessesPawnChange);
		}
	}
}

void UInv_EquipmentComponent::OnPossessesPawnChange(APawn* Pawn, APawn* NewPawn)
{
	ACharacter* Character = Cast<ACharacter>(PlayerController->GetPawn());
	if (IsValid(Character))
	{
		SkeletalMeshComponent = Character->GetMesh();
	}
	InitInventoryComponent();
}

void UInv_EquipmentComponent::InitInventoryComponent()
{
	InventoryComponent = UInv_InventoryStatics::GetInventoryComponent(PlayerController.Get());
	if (!InventoryComponent.IsValid())
		return;
	if (!InventoryComponent->OnItemEquipped.IsAlreadyBound(this, &UInv_EquipmentComponent::OnItemEquipped))
	{
		InventoryComponent->OnItemEquipped.AddDynamic(this, &UInv_EquipmentComponent::OnItemEquipped);
	}
	
	if (!InventoryComponent->OnItemUnequipped.IsAlreadyBound(this, &UInv_EquipmentComponent::OnItemUnequipped))
	{
		InventoryComponent->OnItemUnequipped.AddDynamic(this, &UInv_EquipmentComponent::OnItemUnequipped);
	}
}

AInv_EquipActor* UInv_EquipmentComponent::SpawnEquipActor(FInv_EquipmentFragment* EquipmentFragment,
	const FInv_ItemManifest& Manifest, USkeletalMeshComponent* SkeletalMesh) const
{
	AInv_EquipActor* EquipActor = EquipmentFragment->SpawnAttachActor(SkeletalMesh);	
	EquipActor->SetEquipTag(EquipmentFragment->GetEquipTag());
	EquipActor->SetOwner(GetOwner());
	EquipmentFragment->SetEquipActor(EquipActor);
	return EquipActor;
}

AInv_EquipActor* UInv_EquipmentComponent::FindEquipActor(const FGameplayTag& Tag)
{
	auto FoundActor = EquipActors.FindByPredicate([&Tag](const AInv_EquipActor* EquipActor)
	{
		return EquipActor->GetEquipTag().MatchesTagExact(Tag);
	});
	return FoundActor ? *FoundActor : nullptr;
}

void UInv_EquipmentComponent::RemoveEquipActor(const FGameplayTag& Tag)
{
	AInv_EquipActor* FoundActor = FindEquipActor(Tag);
	if (!IsValid(FoundActor))
		return;
	EquipActors.Remove(FoundActor);
	FoundActor->Destroy();
}

void UInv_EquipmentComponent::OnItemEquipped(UInv_InventoryItem* InventoryItem)
{
	if (!IsValid(InventoryItem))
		return;
	FInv_ItemManifest& ItemManifest = InventoryItem->GetItemManifestMutable();
	FInv_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FInv_EquipmentFragment>();
	if (!EquipmentFragment)
		return;
	if (!bIsProxy)
	{
		if (!PlayerController->HasAuthority())
			return;
		EquipmentFragment->OnEquip(PlayerController.Get());
	}
	if (!SkeletalMeshComponent.IsValid())
		return;
	AInv_EquipActor* EquipActor = SpawnEquipActor(EquipmentFragment, ItemManifest, SkeletalMeshComponent.Get());
	if (bIsProxy)
	{
		EquipActor->SetReplicates(false);
	}
	EquipActors.Add(EquipActor);
}

void UInv_EquipmentComponent::OnItemUnequipped(UInv_InventoryItem* InventoryItem)
{
	if (!IsValid(InventoryItem))
		return;
	FInv_ItemManifest& ItemManifest = InventoryItem->GetItemManifestMutable();
	FInv_EquipmentFragment* EquipmentFragment = ItemManifest.GetFragmentOfTypeMutable<FInv_EquipmentFragment>();
	if (!EquipmentFragment)
		return;
	if (!bIsProxy)
	{
		if (!PlayerController->HasAuthority())
			return;
		EquipmentFragment->OnUnEquip(PlayerController.Get());
	}
	RemoveEquipActor(EquipmentFragment->GetEquipTag());
}
