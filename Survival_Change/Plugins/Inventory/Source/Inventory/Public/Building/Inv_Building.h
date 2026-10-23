#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inv_Building.generated.h"

class UInv_HighlightableStaticMesh;

UCLASS()
class INVENTORY_API AInv_Building : public AActor
{
	GENERATED_BODY()

public:
	AInv_Building();
	virtual FString GetOpenMessage() const;
	virtual FString GetBuildingName() const;
	virtual TArray<FText> GetMessages() const;
	
protected:
	virtual void BeginPlay() override;
	FString OpenMessage;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TArray<FText> Messages;
	
	FString Name;
private:
	UPROPERTY(EditAnywhere)
	TObjectPtr<UInv_HighlightableStaticMesh> StaticMesh;
};
