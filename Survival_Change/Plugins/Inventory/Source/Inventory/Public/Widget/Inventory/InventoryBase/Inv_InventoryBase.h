

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/Inv_GridTypes.h"
#include "Inv_InventoryBase.generated.h"


class UInv_InventoryGrid;
class UInv_InventoryComponent;
class UInv_HoverItem;
class UInv_ItemComponent;
class UInv_InventoryItem;

UCLASS()
class INVENTORY_API UInv_InventoryBase : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual FInv_SlotAvailabilityResult HasRoomForItem(UInv_ItemComponent* InventoryItem) const {return FInv_SlotAvailabilityResult();}
	virtual void OnItemHovered(UInv_InventoryItem* InventoryItem) {}
	virtual void OnItemUnhovered() {}
	virtual bool HasHoveredItem() const { return false; }
	virtual UInv_HoverItem* GetHoverItem() const { return nullptr; }
	virtual float GetTileSize() const { return 0.0f; }
	virtual void SetInventoryComponent(UInv_InventoryComponent* Inventory) {}
	virtual UInv_InventoryGrid* GetOtherGrid() const { return nullptr; }
	virtual UInv_InventoryGrid* GetInventoryGrid() const { return nullptr; }
	virtual void ClearHoverItem() const {};
};
