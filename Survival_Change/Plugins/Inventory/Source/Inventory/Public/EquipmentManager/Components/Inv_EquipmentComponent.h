// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Inv_EquipmentComponent.generated.h"

struct FGameplayTag;
struct FInv_ItemManifest;
struct FInv_EquipmentFragment;
class AInv_EquipActor;
class UInv_InventoryComponent;
class UInv_InventoryItem;
class USkeletalMeshComponent;
class APlayerController;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_EquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	void SetOwningSkeletalMeshComponent(USkeletalMeshComponent* SkeletalMesh);
	void SetProxy(bool bProxy) { bIsProxy = bProxy; }
	bool IsProxy() const { return bIsProxy; }
	void InitializeOwner(APlayerController* PC);
	
protected:
	virtual void BeginPlay() override;

private:
	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<APlayerController> PlayerController;
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	
	UFUNCTION()
	void OnItemEquipped(UInv_InventoryItem* InventoryItem);
	
	UFUNCTION()
	void OnItemUnequipped(UInv_InventoryItem* InventoryItem);
	
	void InitInventoryComponent();
	
	AInv_EquipActor* SpawnEquipActor(FInv_EquipmentFragment* EquipmentFragment, const FInv_ItemManifest& Manifest, USkeletalMeshComponent* SkeletalMeshComponent);
	
	UPROPERTY()
	TArray<TObjectPtr<AInv_EquipActor>> EquipActors;
	
	AInv_EquipActor* FindEquipActor(const FGameplayTag& Tag);
	
	void RemoveEquipActor(const FGameplayTag& Tag);
	void InitPlayerController();
	
	UFUNCTION()
	void OnPossessesPawnChange(APawn* Pawn, APawn* NewPawn);
	
	bool bIsProxy{false};
};
