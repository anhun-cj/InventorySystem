#pragma once

#include "CoreMinimal.h"
#include "../InventoryBase/Inv_InventoryBase.h"
#include "Inv_SpatialInventory.generated.h"

class UInv_InventoryComponent;
class UInv_EquippedSlotItem;
struct FGameplayTag;
class UInv_EquippedGridSlot;
class UInv_ItemDescription;
class UCanvasPanel;
class UInv_InventoryGrid;
class UWidgetSwitcher;
class UButton;

UCLASS()
class INVENTORY_API UInv_SpatialInventory : public UInv_InventoryBase
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	virtual FInv_SlotAvailabilityResult HasRoomForItem(UInv_ItemComponent* ItemComponent) const override;
	virtual void OnItemHovered(UInv_InventoryItem* InventoryItem) override;
	virtual void OnItemUnhovered() override;
	virtual bool HasHoveredItem() const override;
	virtual UInv_HoverItem* GetHoverItem() const override;
	virtual float GetTileSize() const override;
	
	virtual  void SetInventoryComponent(UInv_InventoryComponent* Inventory) override;
	UInv_InventoryItem* FindNoFillItemByType(const FGameplayTag& Tag, TSet<UInv_InventoryItem*> Items) const;
	UFUNCTION(BlueprintCallable)
	UInv_InventoryGrid* GetInventoryGrid() const { return Grid_All;}
private:
	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;
	
	UPROPERTY()
	TArray<TObjectPtr<UInv_EquippedGridSlot>> Equipments;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> Switcher;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> Grid_All;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Equippable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Consumable;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> Button_Craftable;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_ItemDescription> ItemDescriptionClass;
	
	UPROPERTY()
	TObjectPtr<UInv_ItemDescription> ItemDescription;
	
	UInv_ItemDescription* GetItemDescription();
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DescriptionTimerDelay{0.5f};
	
	TWeakObjectPtr<UInv_InventoryGrid> ActiveGrid;
	
	FTimerHandle DescriptionTimer;

	UFUNCTION()
	void ShowAll();

	UFUNCTION()
	void ShowConsumable();

	UFUNCTION()
	void ShowCraftable();
	
	UFUNCTION()
	void EquippedGridSlotClicked(UInv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTag);
	
	UFUNCTION()
	void EquippedSlottedItemClicked(UInv_EquippedSlotItem* EquippedSlotItem);

	void DisableButton(UButton* Button) const;

	void SetActiveGrid(UInv_InventoryGrid* Grid, UButton* Button);
	
	void SetItemDescriptionSizeAndPosition(UInv_ItemDescription* Description, UCanvasPanel* Canvas) const;
	
	bool CanEquipHoverItem(UInv_EquippedGridSlot* Equipment, const FGameplayTag& Tag) const;
	
	UInv_EquippedGridSlot* FindSlotWithEquippedItem(UInv_InventoryItem* Item) const;
	
	void ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot);
	
	void RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem);
	
	void MakeEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem, UInv_EquippedGridSlot* EquippedGridSlot, UInv_InventoryItem* Item);
	
	void BroadcastSlotClickedDelegates(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);
};
