#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inv_OptionMessage.generated.h"

UCLASS()
class INVENTORY_API UInv_OptionMessage : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetMessage(const FText& Message);
};
