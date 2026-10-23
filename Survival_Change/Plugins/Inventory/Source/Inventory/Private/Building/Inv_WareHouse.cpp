
#include "Building/Inv_WareHouse.h"

#include "InventoryManagement/Components/Inv_InventoryComponent.h"


AInv_WareHouse::AInv_WareHouse()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	Inventory = CreateDefaultSubobject<UInv_InventoryComponent>("Inventory");
	OpenMessage = FString(TEXT("F 打开仓库"));
	Messages.Add(FText::FromString(TEXT("F 打开仓库")));
	Name = FString(TEXT("仓库"));
}

