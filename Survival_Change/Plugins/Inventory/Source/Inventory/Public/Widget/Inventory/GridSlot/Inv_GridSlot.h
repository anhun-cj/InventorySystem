

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inv_GridSlot.generated.h"

class UInv_InventoryGrid;
class UInv_HoverItem;
class UInv_ItemPopUp;
class UInv_InventoryItem;
class UImage;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FGridSlotEvent, int32, GridIndex, const FPointerEvent&, MouseEvent);

UCLASS()
class INVENTORY_API UInv_GridSlot : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnMouseEnter(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent, UDragDropOperation*& OutOperation) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;
	
	int32 GetTileIndex() const { return TileIndex; }
	void SetTileIndex(int32 Index) { TileIndex = Index; }
	TWeakObjectPtr<UInv_InventoryItem> GetInventoryItem() const { return InventoryItem; }
	void SetInventoryItem(UInv_InventoryItem* Item);
	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(int32 Count) { StackCount = Count; }
	void SetUpperLeftIndex(int32 Index) { UpperLeftIndex = Index; }
	int32 GetUpperLeftIndex() const { return UpperLeftIndex; }
	bool IsAvailable() const { return bAvailable; }
	void SetAvailable(bool Available) { bAvailable = Available; }
	void SetItemPopUp(UInv_ItemPopUp* PopUp);
	UInv_ItemPopUp* GetItemPopUp() const;
	
	void SetInventoryGrid(UInv_InventoryGrid* Grid);
	
	void SetGreen() const;
	void SetRed() const;
	void SetNormal() const;
	
	void SetUnoccupiedTexture() const;
	void SetOccupiedTexture() const;
	
	FGridSlotEvent GridSlotClicked;
	FGridSlotEvent GridSlotHoverer;
	FGridSlotEvent GridSlotUnhovered;
private:
	int32 TileIndex{INDEX_NONE};
	int32 StackCount{0};
	int32 UpperLeftIndex{INDEX_NONE};
	bool bAvailable{true};
	bool bIsMouseDown{false};
	
	TWeakObjectPtr<UInv_InventoryItem> InventoryItem;

	TWeakObjectPtr<UInv_ItemPopUp> ItemPopUp;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_GridSlot;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Green;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Red;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_Unoccupied;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_Occupied;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_Selected;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FSlateBrush Brush_GrayedOut;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_HoverItem> HoverItemClass;
	
	TWeakObjectPtr<UInv_InventoryGrid> InventoryGrid;
	
	UFUNCTION()
	void OnItemPopUpDestruct(UUserWidget* Menu);
};
