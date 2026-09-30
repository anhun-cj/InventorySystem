// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Inv_SlottedItem.h"
#include "Inv_EquippedSlotItem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEquippedGridSlotItemClicked, UInv_EquippedSlotItem*, EquippedItem);

UCLASS()
class INVENTORY_API UInv_EquippedSlotItem : public UInv_SlottedItem
{
	GENERATED_BODY()
public:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	
	FGameplayTag GetEquipmentTag() const{ return EquipmentTag; }
	void SetEquipmentTag(const FGameplayTag& Tag){EquipmentTag = Tag; }
	
	FEquippedGridSlotItemClicked OnEquippedGridSlotItemClicked;
private:
	UPROPERTY()
	FGameplayTag EquipmentTag;
};
