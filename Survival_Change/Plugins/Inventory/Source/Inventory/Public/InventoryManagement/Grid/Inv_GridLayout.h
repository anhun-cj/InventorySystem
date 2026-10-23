#pragma once

#include "CoreMinimal.h"
#include "Types/Inv_GridTypes.h"

struct FInv_ItemManifest;
class UInv_InventoryItem;
class UInv_InventoryGridAsset;

/**
 * 纯数据网格布局计算器。不依赖任何 Widget，也不持有复制状态。
 *
 * 存在的理由：UInv_InventoryGrid 是 UUserWidget，服务端没有它，所以格位索引
 * 过去只能由客户端计算，再经 RPC 原样交给服务端落库，会产生 TOCTOU 错位。
 * 把"算位置"的职责搬到这层纯数据上，服务端即可自行判定。
 *
 * 占用状态由外部提供（服务端从 InventoryList 推导），本结构只负责计算，
 * 因此不需要任何新的复制协议。
 */
struct INVENTORY_API FInv_GridLayout
{
	FInv_GridLayout() = default;
	FInv_GridLayout(int32 InCol, int32 InRow);

	/** 按已占用格子重建。Occupancy 不足位按空闲处理。 */
	void Setup(int32 InCol, int32 InRow, const TArray<bool>& InOccupancy);

	/** 已有物品完整占地所需的行数，至少保留 InitialRows。 */
	static int32 GetRequiredRows(const TArray<UInv_InventoryItem*>& Items, int32 Col, int32 InitialRows);

	/** 从一组物品推导占用位图（服务端权威来源） */
	static void BuildOccupancy(const TArray<UInv_InventoryItem*>& Items,
	                           int32 Col, int32 Row, TArray<bool>& OutOccupancy, int32 SubGridIndex = INDEX_NONE);

	int32 GetCol() const { return Col; }
	int32 GetRow() const { return Row; }
	int32 GetSlotCount() const { return Col * Row; }

	bool IsValidIndex(int32 Index) const { return Index >= 0 && Index < GetSlotCount(); }

	/** 格子是否空闲（越界按已占用处理） */
	bool IsSlotFree(int32 Index) const;

	/** 物品尺寸（无 GridFragment 时为 1x1） */
	static FIntPoint GetItemDimensions(const FInv_ItemManifest& Manifest);

	/** StartIndex 起、Dimensions 大小的矩形是否落在当前网格内 */
	bool IsInBounds(int32 StartIndex, const FIntPoint& Dimensions) const;

	/** 该起点能否完整放下（含边界与空闲判定，并排除 CheckedIndices） */
	bool CanPlaceAt(int32 StartIndex, const FIntPoint& Dimensions, const TSet<int32>& CheckedIndices) const;

	/** 展开矩形为具体格子索引；越界或跨行回绕时返回 false */
	bool ForEachIndex(int32 StartIndex, const FIntPoint& Dimensions,
	                  TFunctionRef<bool(int32)> Pred) const;

	/** 该矩形包含的格子索引集合 */
	bool CollectIndices(int32 StartIndex, const FIntPoint& Dimensions, TSet<int32>& OutIndices) const;

	/**
	 * 计算物品能放的位置与数量。
	 * 语义对齐 UInv_InventoryGrid::HasRoomForItem，但数据源是本结构的占用位图。
	 *
	 * @param ItemLookup  索引 -> 该格上的物品（用于堆叠判定）；可为空
	 */
	FInv_SlotAvailabilityResult Query(
		const FInv_ItemManifest& Manifest,
		int32 StackAmountOverride,
		bool bAllStackable,
		const TMap<int32, TWeakObjectPtr<UInv_InventoryItem>>& ItemLookup) const;

	/** 空间不足时按物品尺寸及剩余数量扩行，再返回完整放置计划。 */
	FInv_SlotAvailabilityResult QueryWithGrowth(
		const FInv_ItemManifest& Manifest,
		int32 StackAmountOverride,
		bool bAllStackable,
		const TMap<int32, TWeakObjectPtr<UInv_InventoryItem>>& ItemLookup);

	// 整个库存先合并已有堆叠，再按资产顺序查询各子网格空位。
	static FInv_SlotAvailabilityResult QueryInventory(const TArray<UInv_InventoryItem*>& Items,
		const UInv_InventoryGridAsset& Asset, const FInv_ItemManifest& Manifest,
		int32 StackAmountOverride = INDEX_NONE, bool bAllStackable = true);

	/** 自动扩行，无配置行数上限；无效请求返回 0。 */
	int32 GrowRows(int32 RowsToAdd);

	/** 标记矩形为已占用（供批量落库时立即反映前一个物品的占位） */
	void OccupyEntry(int32 StartIndex, const FIntPoint& Dimensions);

private:
	int32 Col = 0;
	int32 Row = 0;
	TArray<bool> Occupied;
};
