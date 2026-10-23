#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "Inv_HoverItem.generated.h"

/**
 * 物品悬停提示
 */

class UInv_InventoryGrid;
class UTextBlock;
class UInv_InventoryItem;
class UImage;

UCLASS()
class INVENTORY_API UInv_HoverItem : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetImageBrush(const FSlateBrush& Brush) const;
	void UpDateStackCount(const int32 Count);
	
	FGameplayTag GetItemType() const;
	int32 GetStackCount() const { return StackCount; }
	bool IsStackable() const { return bIsStackable; }
	void SetIsStackable(bool bStackable) ;
	UFUNCTION(BlueprintPure)
	int32 GetPreviousGridIndex() const { return PreviousGridIndex; }
	void SetPreviousGridIndex(int32 Index) { PreviousGridIndex = Index; }
	FIntPoint GetGridDimensions() const { return GridDimensions; }
	void SetGridDimensions(const FIntPoint& Dimensions) { GridDimensions = Dimensions; }
	UFUNCTION(BlueprintCallable)
	UInv_InventoryItem* GetInventoryItem() const;
	UFUNCTION(BlueprintCallable)
	UInv_InventoryGrid* GetGrid() const { return Grid.Get(); }
	void SetInventoryGrid(UInv_InventoryGrid* InventoryGrid);
	void SetInventoryItem(UInv_InventoryItem* Item);
	void SetCol(int32 Col) { SourceCol = Col; }
	int32 GetCol() const { return SourceCol; }

	FGameplayTag EquipmentType;
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Image_Icon;
	
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_StackCount;
	
	int32 SourceCol;
	
	int32 PreviousGridIndex{INDEX_NONE};
	FIntPoint GridDimensions;
	TWeakObjectPtr<UInv_InventoryItem> InventoryItem;
	TWeakObjectPtr<UInv_InventoryGrid> Grid;
	bool bIsStackable{false};
	int32 StackCount{0};
};
