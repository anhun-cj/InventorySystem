#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inv_HUDWidget.generated.h"

class UVerticalBox;
class UInv_OptionMessage;
class UInv_InfoMessage;

UCLASS()
class INVENTORY_API UInv_HUDWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual void NativeOnInitialized() override;

	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void ShowPickUpMessage(const FString& Message);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void ShowMessages(const TArray<FText>& Messages);
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void HideMessages();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void HidePickUpMessage();
	
	UFUNCTION(BlueprintImplementableEvent, Category = "Inventory")
	void ClearMessages();
private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UInv_InfoMessage> InfoMessage;

	UFUNCTION()
	void OnNoRoom();
};
