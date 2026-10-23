#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Inv_InventoryGridAsset.generated.h"

UCLASS(BlueprintType, EditInlineNew, DefaultToInstanced)
class INVENTORY_API UInv_InventoryGridConfig : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1"))
	int32 Col{2};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "1"))
	int32 Row{2};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "0"))
	FVector2D PositionOffset{0, 0};
};

UCLASS(BlueprintType)
class INVENTORY_API UInv_InventoryGridAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "1"))
	float TileSize{100};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0.1"))
	float GridLineThickness{2};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	FLinearColor GridLineColor{FLinearColor::White};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory", meta = (ClampMin = "0"))
	float SpaceBetweenElements{5};

	UPROPERTY(Instanced, BlueprintReadOnly, Category = "Inventory")
	TArray<TObjectPtr<UInv_InventoryGridConfig>> Grids;
};
