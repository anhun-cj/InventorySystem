#pragma once

#include "CoreMinimal.h"
#include "Inv_GridTypes.generated.h"

class UInv_InventoryItem;

UENUM(BlueprintType)
enum class EInv_ItemCategory : uint8
{
	Equippable,
	Consumable,
	Craftable,
	None,
};

USTRUCT()
struct FInv_SlotAvailability
{
	GENERATED_BODY()
	
	FInv_SlotAvailability() {}
	FInv_SlotAvailability(int32 Index, int32 Room, bool bItemAtIndex, UInv_InventoryItem* InventoryItem);
	
	int32 Index = INDEX_NONE;
	int32 AmountToFill = 0;
	bool bItemAtIndex = false;
	TWeakObjectPtr<UInv_InventoryItem> Item = nullptr;
};

USTRUCT()
struct FInv_SlotAvailabilityResult
{
	GENERATED_BODY()
	FInv_SlotAvailabilityResult() {}
	
	TWeakObjectPtr<UInv_InventoryItem> Item;
	
	int32 TotalRoomToFill = 0;
	int32 Remainder = 0;
	
	bool bStackable = false;
	
	TArray<FInv_SlotAvailability> SlotAvailabilities;
};

UENUM(BlueprintType)
enum class EInv_TileQuadrant: uint8
{
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
	None,
};

USTRUCT(BlueprintType)
struct FInv_TileParameters
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	FIntPoint TileCoordinates{};
	
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	int32 TileIndex{INDEX_NONE};
	
	UPROPERTY(BlueprintReadWrite, Category = "Inventory")
	EInv_TileQuadrant TileQuadrant{EInv_TileQuadrant::None};
};

inline bool operator==(const FInv_TileParameters& A, const FInv_TileParameters& B)
{
	return A.TileCoordinates == B.TileCoordinates && A.TileIndex == B.TileIndex && A.TileQuadrant == B.TileQuadrant;
}

USTRUCT(BlueprintType)
struct FInv_SpaceQueryResult
{
	GENERATED_BODY()
	//没有物品为真
	bool bHasSpace{false};
	bool bAllowItemDown{false};
	//移动网格范围内的物品，允许交换后需要根据计算后的坐标交换位置
	UPROPERTY()
	TArray<TWeakObjectPtr<UInv_InventoryItem>> ValidItems;
	UPROPERTY()
	TArray<int32> ItemMoveIndices;
	//可以交换网格颜色(true为绿色，false为红色)
	TArray<bool> Green;
	
	TWeakObjectPtr<UInv_InventoryItem> ValidItem = nullptr;
	
	int32 UpperLeftIndex{INDEX_NONE};
};


