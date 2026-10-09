

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Widget/Utils/Inv_WidgetUtils.h"
#include "Inv_InventoryStatics.generated.h"


class UInv_InventoryBase;
class UInv_HoverItem;
class UInv_InventoryItem;
class UInv_ItemComponent;
class UInv_InventoryComponent;
enum class EInv_ItemCategory : uint8;

UCLASS()
class INVENTORY_API UInv_InventoryStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static UInv_InventoryComponent* GetInventoryComponent(const APlayerController* PlayerController);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static EInv_ItemCategory GetItemCategoryForItemComponent(UInv_ItemComponent* ItemComponent);
	
	template<typename T, typename FuncT>
	static void ForEch2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridCol, const FuncT& Function);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static void ItemHovered(UInv_InventoryComponent* InventoryComponent, UInv_InventoryItem* Item);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static void ItemUnhovered(APlayerController* PC);
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	static UInv_HoverItem* GetHoverItem(APlayerController* PC);
	
	static UInv_InventoryBase* GetInventoryWidget(APlayerController* PC);
};

template<typename T, typename FuncT>
void UInv_InventoryStatics::ForEch2D(TArray<T>& Array, int32 Index, const FIntPoint& Range2D, int32 GridCol, const FuncT& Function)
{
	const FIntPoint StartPos = UInv_WidgetUtils::GetPositionFromIndex(Index, GridCol);
	
	for (int j = 0; j < Range2D.Y; ++j)
	{
		for (int i = 0; i < Range2D.X; ++i)
		{
			const FIntPoint Coordinates = StartPos + FIntPoint(i, j);
			const int32 TileIndex = UInv_WidgetUtils::GetIndexFromPosition(Coordinates, GridCol);
			if (Array.IsValidIndex(TileIndex))
				Function(Array[TileIndex]);
		}
	}
}