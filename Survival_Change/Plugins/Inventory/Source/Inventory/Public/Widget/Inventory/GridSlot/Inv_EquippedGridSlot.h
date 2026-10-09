// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inv_GridSlot.h"
#include "Inv_EquippedGridSlot.generated.h"

class UOverlay;
class UInv_EquippedSlotItem;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEquippedGridSlotClicked, UInv_EquippedGridSlot*, GridSlot,
                                             const FGameplayTag&, EquipmentType);

UCLASS()
class INVENTORY_API UInv_EquippedGridSlot : public UInv_GridSlot
{
	GENERATED_BODY()
public:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnDragLeave(const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	UInv_EquippedSlotItem* OnItemEquipped(UInv_InventoryItem* Item, const FGameplayTag& Tag, float TileSize);
	void SetEquippedSlotItem(UInv_EquippedSlotItem* Item){EquippedSlotItem = Item;}
	
	FEquippedGridSlotClicked EquippedGridSlotClicked;
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FGameplayTag EquipmentType;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_EquippedSlotItem> EquippedSlotItemClass;
	
	UPROPERTY()
	TObjectPtr<UInv_EquippedSlotItem> EquippedSlotItem;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UOverlay> Overlay_Root;
};
