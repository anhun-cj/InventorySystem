#pragma once

#include "CoreMinimal.h"
#include "Inv_InventoryGrid.h"
#include "../InventoryBase/Inv_InventoryBase.h"
#include "Inv_SpatialInventory.generated.h"

class UVerticalBox;
class USizeBox;
class UTextBlock;
class UInv_InventoryComponent;
class UInv_EquippedSlotItem;
struct FGameplayTag;
struct FInv_ItemManifest;
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
	virtual void NativeDestruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	
	virtual FInv_SlotAvailabilityResult HasRoomForItem(UInv_ItemComponent* ItemComponent) const override;
	virtual FInv_SlotAvailabilityResult HasRoomForItem(UInv_InventoryItem* InventoryItem) const override;
	virtual void OnItemHovered(UInv_InventoryItem* InventoryItem) override;
	virtual void OnItemUnhovered() override;
	virtual bool HasHoveredItem() const override;
	virtual UInv_HoverItem* GetHoverItem() const override;
	virtual float GetTileSize() const override;
	
	virtual void SetInventoryComponent(UInv_InventoryComponent* Inventory) override;
	UInv_InventoryItem* FindNoFillItemLike(const FInv_ItemManifest& Source, const TSet<UInv_InventoryItem*>& Items) const;
	virtual UInv_InventoryGrid* GetOtherGrid() const override { return Grid_Other;}
	UFUNCTION(BlueprintCallable)
	virtual UInv_InventoryGrid* GetInventoryGrid() const override { return Grid_All;}
	virtual UInv_InventoryGrid* GetOpenInventoryGrid() const override { return OpenInventory; }
	void InventoryBindEvent() const;
	virtual void SetOpenInventory(UInv_InventoryComponent* InInventoryComponent) override;
	
	virtual void ClearHoverItem() const override;
	virtual void RemoveSlotItem(UInv_SlottedItem* Item, UInv_InventoryGrid* TargetGrid) override;
	
	void FinishDragPrediction();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable, Category = "Inventory")
	void SetTextVisibility(ESlateVisibility InVisibility);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TArray<TObjectPtr<UInv_EquippedGridSlot>> Equipments;
private:
	friend class UInv_InventoryGrid;
	friend class UInv_GridSlot;
	friend class UInv_EquippedSlotItem;
	friend class UInv_SlottedItem;

	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> Grid_All;
	
	UPROPERTY()
	TObjectPtr<UInv_InventoryGrid> Grid_Other;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> VerticalBox;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UUserWidget> Survival_ActorEquipment;
	
	UPROPERTY()
	TObjectPtr<UInv_InventoryGrid> OpenInventory;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> BackPackInventory;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> ChestRigInventory;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_BackPack;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_ChestRig;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DescriptionTimerDelay{0.5f};
	
	FTimerHandle DescriptionTimer;

	UFUNCTION()
	void ShowAll() const;

	UFUNCTION()
	void ShowConsumable() const;

	UFUNCTION()
	void ShowCraftable() const;
	
	UFUNCTION()
	void EquippedItem(UInv_EquippedGridSlot* EquippedGridSlot, const FGameplayTag& EquipmentTag) const;
	
	UFUNCTION()
	void EquipItem(UInv_InventoryItem* Item);
	
	UFUNCTION()
	void UnEquipItem(UInv_InventoryItem* Item);

	void DisableButton(UButton* Button) const;

	void SetActiveGrid(UInv_InventoryGrid* Grid, UButton* Button) const;
	
	void SetItemDescriptionSizeAndPosition(UInv_ItemDescription* Description, UCanvasPanel* Canvas) const;
	
	bool CanEquipHoverItem(UInv_EquippedGridSlot* Equipment, const FGameplayTag& Tag) const;
	
	UInv_EquippedGridSlot* FindSlotWithEquippedItem(UInv_InventoryItem* Item) const;
	
	UInv_EquippedGridSlot* FindSlotWithEquippedType(const FGameplayTag& Tag) const;
	
	void ClearSlotItem(UInv_EquippedGridSlot* EquippedGridSlot) const;
	
	void RemoveEquippedSlottedItem(UInv_EquippedSlotItem* EquippedSlotItem) const;
	
	void MakeEquippedSlottedItem(UInv_EquippedGridSlot* EquippedGridSlot, UInv_InventoryItem* Item) const;
	
	void BroadcastSlotClickedDelegates(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip,
		int32 ItemDropIndex, const UInv_InventoryGrid* SourceGrid, const UInv_InventoryGrid* TargetGrid, int32 ItemDropSubGridIndex = INDEX_NONE) const;
};
