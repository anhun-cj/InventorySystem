// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Inv_RequestComponent.generated.h"


class UInv_InventoryComponent;
struct FInv_SpaceQueryResult;
class UInv_InventoryItem;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class INVENTORY_API UInv_RequestComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInv_RequestComponent();
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Inventory")
	void OpenInventory(UInv_InventoryComponent* Inventory);

	UFUNCTION(Server, Reliable, BlueprintCallable)
	void TransferItem(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex, UInv_InventoryComponent* TargetComponent, UInv_InventoryComponent* SourceComponent, int32 SourceIndex, UInv_InventoryItem* TargetContainer = nullptr, UInv_InventoryItem* SourceContainer = nullptr, int32 SourceSubGridIndex = 0, int32 TargetSubGridIndex = 0);
	
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void Server_EquipItem(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip, int32 ItemDropIndex, UInv_InventoryComponent* TargetComponent, UInv_InventoryComponent* SourceComponent, const FGameplayTag& EquipmentType, int32 SourceIndex, UInv_InventoryItem* SourceContainer = nullptr, int32 SourceSubGridIndex = 0, int32 TargetSubGridIndex = 0, bool bFindUnequipSpace = false);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipItem(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip, UInv_InventoryComponent* TargetComponent);
	
	UFUNCTION(Server, Reliable)
	void DropItem(UInv_InventoryItem* Item);
	
	UFUNCTION(Server, Reliable)
	void SplitItem(UInv_InventoryItem* Item, int32 Count, int32 Index, int32 SubGridIndex = 0);
	
	UFUNCTION(Server, Reliable)
	void ConsumeItem(UInv_InventoryItem* Item);
private:
	UFUNCTION(Client, Reliable)
	void Client_DragResult();

	UFUNCTION(Client, Reliable)
	void Client_OpenInventory(UInv_InventoryComponent* Inventory);
};
