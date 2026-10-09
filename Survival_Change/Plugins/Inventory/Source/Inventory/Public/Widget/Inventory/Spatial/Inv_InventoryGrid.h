#pragma once

#include "CoreMinimal.h"
#include "IContentBrowserSingleton.h"
#include "Blueprint/UserWidget.h"
#include "Types/Inv_GridTypes.h"
#include "Inv_InventoryGrid.generated.h"

class UWrapBox;
class UInv_ItemPopUp;
enum class EInv_GridSlotState : uint8;
class UInv_HoverItem;
struct FGameplayTag;
struct FInv_ImageFragment;
struct FInv_GridFragment;
class UInv_SlottedItem;
class UInv_ItemComponent;
struct FInv_ItemManifest;
class UInv_InventoryComponent;
class UInv_InventoryBase;
class UCanvasPanel;
class UInv_GridSlot;

UCLASS()
class INVENTORY_API UInv_InventoryGrid : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

	EInv_ItemCategory GetItemCategory() const { return ItemCategory; }
	
	FInv_SlotAvailabilityResult HasRoomForItem(const UInv_ItemComponent* ItemComponent, const bool bAllStackable = true);
	
	void ShowCursor();
	void HideCursor();
	void SetOwningCanvasPanel(UCanvasPanel* Canvas);
	void DropItem();
	bool HasHoveredItem() const;
	UInv_HoverItem* GetHoverItem() const;
	float GetTileSize() const { return TileSize; };
	void ClearHoverItem();
	void AssignHoverItem(UInv_InventoryItem* InventoryItem);
	void OnHide();
	void SetHoverItem(UInv_HoverItem* Item);
	UFUNCTION(BlueprintCallable)
	FInv_SpaceQueryResult GetResult() const {return CurrentQueryResult;}
	UFUNCTION(BlueprintCallable)
	int32 GetItemDropIndex() const{return ItemDropIndex;}
	void UnHighlightSlots();
	void BindInventoryEvent(UInv_InventoryComponent* Inventory);
	void UnBindInventoryEvent();
	UInv_InventoryComponent* GetInventoryComponent() const { return InventoryComponent.Get(); }
	
	void SetHoverSourceMenu(UInv_InventoryBase* InMenu);
	UInv_InventoryBase* GetHoverSourceMenu() const {return HoverSourceMenu.Get();};
	
	UFUNCTION()
	void RemoveItem(UInv_InventoryItem* Item);
	
	UFUNCTION()
	void AddItem(UInv_InventoryItem* Item);

	void InitInventory();
	void ClearInventory();
	void ConstructGridSlots();
	int32 GetCol() const { return Col; }
private:

	TWeakObjectPtr<UInv_InventoryComponent> InventoryComponent;
	TWeakObjectPtr<UInv_InventoryBase> HoverSourceMenu;
	TWeakObjectPtr<UCanvasPanel> OwningCanvasPanel;
	
	void ConstructGrid();
	
	FInv_SlotAvailabilityResult HasRoomForItem(const UInv_InventoryItem* Item, const int32 StackAmountOverride = -1, const bool bAllStackable = true);
	FInv_SlotAvailabilityResult HasRoomForItem(const FInv_ItemManifest& Manifest, const int32 StackAmountOverride = -1, const bool bAllStackable = true);
	void AddItemToIndices(UInv_InventoryItem* Item);
	bool MatchesCategory(UInv_InventoryItem* Item) const;
	
	FVector2D GetDrawSize(const FInv_GridFragment* GridFragment) const;
	void SetSlottedItem(const UInv_SlottedItem* SlottedItem, const FInv_GridFragment* GridFragment, const FInv_ImageFragment* ImageFragment) const;
	void AddItemAtIndex(UInv_InventoryItem* Item, const int32 Index, const bool bStackable, const int32 StackAmount);
	UInv_SlottedItem* CreateSlottedItem(UInv_InventoryItem* Item, 
		const bool bStackable, 
		const int32 StackAmount, 
		const FInv_GridFragment* GridFragment, 
		const FInv_ImageFragment* ImageFragment,
		const int32 Index);
	
	void AddSlottedItemToCanvas(const int32 Index, const FInv_GridFragment* GridFragment, UInv_SlottedItem* SlottedItem) const;
	
	void UpdateGridSlots(UInv_InventoryItem* Item, int32 Index, bool bStackableItem, const int32 StackAmount);
	
	bool IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const;
	bool HasRoomAtIndex(const UInv_GridSlot* GridSlot, 
		const FIntPoint& Dimensions,
		const TSet<int32>& CheckedIndices,
		TSet<int32>& OutTentativelyClaimed,
		const FGameplayTag& ItemType,
		const int32 MaxStackCount);
	FIntPoint GetItemDimensions(const FInv_ItemManifest& Manifest) const;
	bool CheckSlotContains(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot, const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
		const FGameplayTag& ItemType, const int32 MaxStackCount) const;
	bool HasValidItem(const UInv_GridSlot* GridSlot) const;
	bool IsUpperLeftSlot(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot) const;
	bool DoesItemMatch(const UInv_InventoryItem* SubItem, const FInv_ItemManifest& Source) const;
	bool IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const;
	int32 DetermineFillAmountFotSlot(const bool bStackable, const int32 MaxStackCount, const int32 AmountToFill, const UInv_GridSlot* GridSlot) const;
	int32 GetStackAmount(const UInv_GridSlot* GridSlot) const;
	bool IsRightClick(const FPointerEvent& MouseEvent) const;
	bool IsLeftClick(const FPointerEvent& MouseEvent) const;
	void PickUp(UInv_InventoryItem* ClickedInventoryItem, int32 GridIndex);
	
	void AssignHoverItem(UInv_InventoryItem* InventoryItem, int32 GridIndex, int32 PreviousGridIndex);
	void UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition);
	FIntPoint CalculateHoveredCoordinates(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	EInv_TileQuadrant CalculateTileQuadrant(const FVector2D& CanvasPosition, const FVector2D& MousePosition) const;
	void OnTileParametersUpdated(const FInv_TileParameters& Parameters);
	FIntPoint CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions, const EInv_TileQuadrant Quadrant) const;
	FInv_SpaceQueryResult CheckHoverPosition(const FIntPoint& Position, const FIntPoint& Dimensions);
	bool CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize, const FVector2D& Location);
	void HighLightSlots(const int32 Index, const FIntPoint& Dimensions);
	void UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions);
	void PutDownOnIndex(const int32 Index);
	UUserWidget* GetVisibleCurUserWidget();
	UUserWidget* GetHiddenCurUserWidget();
	bool IsSameStackable(UInv_InventoryItem* ClickedInventoryItem) const;
	void SwapWithHoverItem(UInv_InventoryItem* ClickedInventoryItem, const int32 GridIndex);
	bool ShouldSwapStackCount(const int32 RoomInClickedSlot, const int32 HoveredStackCount, const int32 MaxStackCount) const;
	void SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldConsumeHoverItemStacks(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const;
	void ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount, const int32 Index);
	bool ShouldFillInStack(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const;
	void FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index);
	void CreateItemPopUp(const int32 GridIndex);
	void PutHoverItemBack();
	//检测移动位置附近物品是否可以交换
	bool CheckHoverItemMoveItems(UInv_GridSlot* GridSlot, const FIntPoint& SourcePosition, const FIntPoint& SourceDimensions) const;
	FIntPoint CalculateMoveItemPosition(UInv_GridSlot* GridSlot, const FVector2D& TargetPosition, const FVector2D& SourcePosition) const;
	
	UPROPERTY()
	TObjectPtr<UInv_ItemPopUp> ItemPopUp;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_ItemPopUp> ItemPopUpClass;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UUserWidget> VisibleCursorWidgetClass;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UUserWidget> HiddenCursorWidgetClass;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> VisibleCursorWidget;
	
	UPROPERTY()
	TObjectPtr<UUserWidget> HiddenCursorWidget;
	
	UFUNCTION()
	void OnItemChange(UInv_InventoryItem* Item);
	
	UFUNCTION()
	void RemoveItemFormGrid(UInv_InventoryItem* InventoryItem, int32 GridIndex);
	
	UFUNCTION()
	void OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent);
	
	UFUNCTION()
	void OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent);
	
	UFUNCTION()
	void OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent);
	
	UFUNCTION()
	void OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent);

	UFUNCTION()
	void OnPopUpMenuSplit(int32 SplitAmount, int32 Index);
	
	UFUNCTION()
	void OnPopUpMenuDrop(int32 Index);
	
	UFUNCTION()
	void OnPopUpMenuConsume(int32 Index);
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"), Category = "Inventory")
	EInv_ItemCategory ItemCategory;

	UPROPERTY()
	TArray<TObjectPtr<UInv_GridSlot>> GridSlots;

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_GridSlot> GridSlotClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> CanvasPanel;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWrapBox> WrapBox_GridSlots;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_SlottedItem> SlottedItemClass;

	UPROPERTY()
	TMap<int32, TObjectPtr<UInv_SlottedItem>> SlottedItems;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 Row;//行

	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 Col;//列

	UPROPERTY(EditAnywhere, Category = "Inventory")
	float TileSize;
	
	UPROPERTY()
	TWeakObjectPtr<UInv_HoverItem> HoverItem;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FVector2D ItemPopUpOffset;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_HoverItem> HoverItemClass;
	
	FInv_TileParameters TileParameters;
	FInv_TileParameters LastTileParameters;
	//放置拿取物品的索引
	int32 ItemDropIndex{INDEX_NONE};
	UPROPERTY(Replicated)
	FInv_SpaceQueryResult CurrentQueryResult;
	bool bMouseWithinCanvas;
	bool bLastMouseWithinCanvas;
	
	int32 LastHighlightedIndex{INDEX_NONE};
	FIntPoint LastHighlightedDimensions;
	
	FTimerHandle TimerHandle;
};