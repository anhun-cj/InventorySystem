#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inv_OpenInventoryWidget.generated.h"


class UInv_InventoryGrid;
class UInv_InventoryComponent;
class UInv_InventoryBase;
class UCanvasPanel;
class UTextBlock;

UCLASS()
class INVENTORY_API UInv_OpenInventoryWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	UInv_InventoryGrid* InitInventory(UInv_InventoryComponent* Inventory, UInv_InventoryBase* Menu, UCanvasPanel* Canvas);
private:
	UPROPERTY(EditAnywhere, meta = (BindWidget))
	TObjectPtr<UInv_InventoryGrid> Grid;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> Text_BuildingName;
};
