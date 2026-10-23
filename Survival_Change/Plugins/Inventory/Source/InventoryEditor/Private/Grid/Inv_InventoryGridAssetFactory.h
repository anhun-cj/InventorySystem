#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "Inv_InventoryGridAssetFactory.generated.h"

UCLASS()
class UInv_InventoryGridAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UInv_InventoryGridAssetFactory();
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags,
		UObject* Context, FFeedbackContext* Warn) override;
};
