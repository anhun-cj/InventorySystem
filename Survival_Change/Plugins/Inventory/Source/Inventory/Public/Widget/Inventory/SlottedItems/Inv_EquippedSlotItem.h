// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inv_SlottedItem.h"
#include "Inv_EquippedSlotItem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEquippedGridSlotItemClicked, UInv_EquippedSlotItem*, EquippedItem, UInv_InventoryItem*, Item);

UCLASS()
class INVENTORY_API UInv_EquippedSlotItem : public UInv_SlottedItem
{
	GENERATED_BODY()
public:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	FGameplayTag GetEquipmentTag() const{ return EquipmentTag; }
	void SetEquipmentTag(const FGameplayTag& Tag){EquipmentTag = Tag; }
	void SetGridSlot(UInv_EquippedGridSlot* InSlot);
	
	FEquippedGridSlotItemClicked OnEquippedItem;
private:
	UPROPERTY()
	FGameplayTag EquipmentTag;
	
	TWeakObjectPtr<UInv_EquippedGridSlot> GridSlot;
};
