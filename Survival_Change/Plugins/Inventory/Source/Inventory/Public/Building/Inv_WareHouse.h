#pragma once

#include "CoreMinimal.h"
#include "Inv_Building.h"
#include "Inv_WareHouse.generated.h"

class UInv_InventoryComponent;

UCLASS()
class INVENTORY_API AInv_WareHouse : public AInv_Building
{
	GENERATED_BODY()

public:
	AInv_WareHouse();
	
private:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UInv_InventoryComponent> Inventory;
};
