// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Inv_WareHouseWidget.generated.h"


struct FGameplayTag;
class UInv_EquippedSlotItem;
class UInv_EquippedGridSlot;
class UInv_InventoryGrid;
class UInv_InventoryComponent;

UCLASS()
class INVENTORY_API UInv_WareHouseWidget : public UInv_InventoryBase
{
	GENERATED_BODY()
public:
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual void NativeOnInitialized() override;
	virtual UInv_HoverItem* GetHoverItem() const override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	void InitWareHouseWidget(UInv_InventoryComponent* PlayerInventoryComponent, UInv_InventoryComponent* WareHouseInventoryComponent);
	virtual UInv_InventoryGrid* GetOtherGrid() const override;
	virtual UInv_InventoryGrid* GetInventoryGrid() const override{ return PlayerInventoryGrid; }
	void UnBindEvent() const;

	virtual void ClearHoverItem() const override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<TObjectPtr<UInv_EquippedGridSlot>> Equipments;
private:
	TWeakObjectPtr<UInv_InventoryComponent> PlayerInventory;
	
	TWeakObjectPtr<UInv_InventoryComponent> WareHouseInventory;
	
	TWeakObjectPtr<UInv_InventoryComponent> OtherInventory;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> WareHouseInventoryGrid;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> PlayerInventoryGrid;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> Grid_Other;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUserWidget> Survival_ActorEquipment;
	
	UFUNCTION()
	void EquipItem(UInv_InventoryItem* Item);
	
	UFUNCTION()
	void UnEquipItem(UInv_InventoryItem* Item);
	
	void ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot) const;
	
	void RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem) const;
	
	UInv_EquippedGridSlot* FindSlotWithEquippedItem(UInv_InventoryItem* Item) const;
	
	UInv_EquippedGridSlot* FindSlotWithEquippedType(const FGameplayTag& Tag) const;
};
