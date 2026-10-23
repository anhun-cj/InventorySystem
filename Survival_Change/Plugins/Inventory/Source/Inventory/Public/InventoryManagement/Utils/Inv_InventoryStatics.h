

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

	template<typename T, typename FuncT>
	static void ForEach2D(TArray<T>& Array, const FIntPoint& StartCoordinate, const FIntPoint& Range2D, int32 GridCol, const FuncT& Function);
	
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
	ForEach2D(Array, UInv_WidgetUtils::GetPositionFromIndex(Index, GridCol), Range2D, GridCol, Function);
}

template<typename T, typename FuncT>
void UInv_InventoryStatics::ForEach2D(TArray<T>& Array, const FIntPoint& StartCoordinate, const FIntPoint& Range2D, int32 GridCol, const FuncT& Function)
{
	if (GridCol <= 0 || Array.Num() <= 0)
		return;

	const int32 GridRow = Array.Num() / GridCol;

	for (int32 j = 0; j < Range2D.Y; ++j)
	{
		const int32 Y = StartCoordinate.Y + j;
		if (Y < 0 || Y >= GridRow)
			continue;

		for (int32 i = 0; i < Range2D.X; ++i)
		{
			const int32 X = StartCoordinate.X + i;
			if (X < 0 || X >= GridCol)
				continue;

			Function(Array[UInv_WidgetUtils::GetIndexFromPosition(FIntPoint(X, Y), GridCol)]);
		}
	}
}
