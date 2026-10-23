#include "InventoryManagement/Grid/Inv_GridLayout.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"

#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Manifest/Inv_ItemManifest.h"

FInv_GridLayout::FInv_GridLayout(int32 InCol, int32 InRow)
{
	Setup(InCol, InRow, TArray<bool>());
}

void FInv_GridLayout::Setup(int32 InCol, int32 InRow, const TArray<bool>& InOccupancy)
{
	Col = FMath::Max(1, InCol);
	Row = FMath::Max(1, InRow);

	Occupied.SetNumZeroed(GetSlotCount());
	const int32 CopyCount = FMath::Min(InOccupancy.Num(), GetSlotCount());
	for (int32 i = 0; i < CopyCount; ++i)
	{
		Occupied[i] = InOccupancy[i];
	}
}

int32 FInv_GridLayout::GetRequiredRows(const TArray<UInv_InventoryItem*>& Items, int32 Col, int32 InitialRows)
{
	if (Col <= 0)
		return 0;
	int32 RequiredRows = FMath::Max(1, InitialRows);
	for (const UInv_InventoryItem* Item : Items)
	{
		if (IsValid(Item) && Item->GetGridIndex() >= 0)
			RequiredRows = FMath::Max(RequiredRows, Item->GetGridIndex() / Col
				+ GetItemDimensions(Item->GetItemManifest()).Y);
	}
	return RequiredRows;
}

bool FInv_GridLayout::IsSlotFree(int32 Index) const
{
	if (!IsValidIndex(Index))
		return false;
	return !Occupied[Index];
}

FIntPoint FInv_GridLayout::GetItemDimensions(const FInv_ItemManifest& Manifest)
{
	const FInv_GridFragment* GridFragment = Manifest.GetFragmentOfType<FInv_GridFragment>();
	return GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
}

bool FInv_GridLayout::ForEachIndex(int32 StartIndex, const FIntPoint& Dimensions,
                                   TFunctionRef<bool(int32)> Pred) const
{
	if (!IsValidIndex(StartIndex) || Dimensions.X <= 0 || Dimensions.Y <= 0)
		return false;

	const int32 StartCol = StartIndex % Col;
	for (int32 y = 0; y < Dimensions.Y; ++y)
	{
		for (int32 x = 0; x < Dimensions.X; ++x)
		{
			const int32 Index = StartIndex + y * Col + x;
			if (!IsValidIndex(Index))
				return false;
			// 跨行回绕说明矩形超出了右边界
			if ((Index % Col) != (StartCol + x))
				return false;
			if (!Pred(Index))
				return false;
		}
	}
	return true;
}

bool FInv_GridLayout::CollectIndices(int32 StartIndex, const FIntPoint& Dimensions, TSet<int32>& OutIndices) const
{
	OutIndices.Reset();
	return ForEachIndex(StartIndex, Dimensions, [&OutIndices](int32 Index)
	{
		OutIndices.Add(Index);
		return true;
	});
}

bool FInv_GridLayout::IsInBounds(int32 StartIndex, const FIntPoint& Dimensions) const
{
	if (StartIndex < 0 || StartIndex >= GetSlotCount() || Dimensions.X <= 0 || Dimensions.Y <= 0)
		return false;

	const int32 EndCol = StartIndex % Col + Dimensions.X;
	const int32 EndRow = StartIndex / Col + Dimensions.Y;
	return EndCol <= Col && EndRow <= Row;
}

bool FInv_GridLayout::CanPlaceAt(int32 StartIndex, const FIntPoint& Dimensions,
                                 const TSet<int32>& CheckedIndices) const
{
	if (!IsInBounds(StartIndex, Dimensions))
		return false;

	return ForEachIndex(StartIndex, Dimensions, [this, &CheckedIndices](int32 Index)
	{
		if (CheckedIndices.Contains(Index))
			return false;
		return IsSlotFree(Index);
	});
}

int32 FInv_GridLayout::GrowRows(int32 RowsToAdd)
{
	if (RowsToAdd <= 0 || Col <= 0)
		return 0;

	Row += RowsToAdd;
	Occupied.AddZeroed(RowsToAdd * Col);
	return RowsToAdd;
}

void FInv_GridLayout::OccupyEntry(int32 StartIndex, const FIntPoint& Dimensions)
{
	ForEachIndex(StartIndex, Dimensions, [this](int32 Index)
	{
		Occupied[Index] = true;
		return true;
	});
}

void FInv_GridLayout::BuildOccupancy(const TArray<UInv_InventoryItem*>& Items,
                                     int32 Col, int32 Row, TArray<bool>& OutOccupancy, int32 SubGridIndex)
{
	OutOccupancy.Reset();
	if (Col <= 0 || Row <= 0)
		return;

	OutOccupancy.SetNumZeroed(Col * Row);
	FInv_GridLayout Tmp(Col, Row);

	for (const UInv_InventoryItem* Item : Items)
	{
		if (!IsValid(Item))
			continue;

		const int32 GridIndex = Item->GetGridIndex();
		if (GridIndex == INDEX_NONE || (SubGridIndex != INDEX_NONE && Item->GetSubGridIndex() != SubGridIndex))
			continue;

		const FIntPoint Dim = GetItemDimensions(Item->GetItemManifest());
		Tmp.ForEachIndex(GridIndex, Dim, [&OutOccupancy](int32 Index)
		{
			if (Index >= 0 && Index < OutOccupancy.Num())
			{
				OutOccupancy[Index] = true;
			}
			return true;
		});
	}
}

FInv_SlotAvailabilityResult FInv_GridLayout::Query(
	const FInv_ItemManifest& Manifest,
	int32 StackAmountOverride,
	bool bAllStackable,
	const TMap<int32, TWeakObjectPtr<UInv_InventoryItem>>& ItemLookup) const
{
	FInv_SlotAvailabilityResult Result;

	const FInv_StackableFragment* StackableFragment = Manifest.GetFragmentOfType<FInv_StackableFragment>();
	Result.bStackable = (StackableFragment != nullptr);

	const int32 MaxStackCount = StackableFragment ? StackableFragment->GetMaxStackCount() : 1;
	int32 AmountToFill = StackableFragment ? StackableFragment->GetStackCount() : 1;

	if (StackAmountOverride != -1 && Result.bStackable)
	{
		AmountToFill = StackAmountOverride;
	}
	Result.Remainder = FMath::Max(0, AmountToFill);
	const FIntPoint Dimensions = GetItemDimensions(Manifest);
	if (Col <= 0 || Dimensions.X <= 0 || Dimensions.Y <= 0 || Dimensions.X > Col
		|| MaxStackCount <= 0 || AmountToFill <= 0)
		return Result;
	
	//判断已有物品是否可以堆叠
	if (bAllStackable && Result.bStackable)
	{
		for (const TPair<int32, TWeakObjectPtr<UInv_InventoryItem>>& Pair : ItemLookup)
		{
			if (AmountToFill == 0)
				break;

			UInv_InventoryItem* Existing = Pair.Value.Get();
			if (!IsValid(Existing))
				continue;
			if (!FInv_ItemManifest::AreSameItem(Existing->GetItemManifest(), Manifest))
				continue;

			const FInv_StackableFragment* ExistingStack =
				Existing->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
			if (!ExistingStack)
				continue;

			const int32 RoomInSlot = MaxStackCount - ExistingStack->GetStackCount();
			if (RoomInSlot <= 0)
				continue;

			const int32 AmountToFillSlot = FMath::Min(AmountToFill, RoomInSlot);
			Result.SlotAvailabilities.Emplace(
				FInv_SlotAvailability(Pair.Key, AmountToFillSlot, true, Existing));
			Result.TotalRoomToFill += AmountToFillSlot;
			AmountToFill -= AmountToFillSlot;
		}

		if (AmountToFill == 0)
		{
			Result.Remainder = 0;
			return Result;
		}
	}

	//找空位
	TSet<int32> CheckedIndices;

	for (int32 Index = 0; Index < GetSlotCount(); ++Index)
	{
		if (AmountToFill == 0)
			break;
		if (CheckedIndices.Contains(Index))
			continue;
		if (!CanPlaceAt(Index, Dimensions, CheckedIndices))
			continue;

		TSet<int32> Reserved;
		if (!CollectIndices(Index, Dimensions, Reserved))
			continue;
		CheckedIndices.Append(Reserved);
		
		const int32 AmountToFillSlot = Result.bStackable ? FMath::Min(AmountToFill, MaxStackCount) : 1;
		if (AmountToFillSlot <= 0)
			continue;

		Result.TotalRoomToFill += AmountToFillSlot;
		Result.SlotAvailabilities.Emplace(
			FInv_SlotAvailability(Index, Result.bStackable ? AmountToFillSlot : 1, false, nullptr));
		AmountToFill -= AmountToFillSlot;
	}

	Result.Remainder = AmountToFill;
	return Result;
}

FInv_SlotAvailabilityResult FInv_GridLayout::QueryWithGrowth(
	const FInv_ItemManifest& Manifest, int32 StackAmountOverride, bool bAllStackable,
	const TMap<int32, TWeakObjectPtr<UInv_InventoryItem>>& ItemLookup)
{
	FInv_SlotAvailabilityResult Result = Query(Manifest, StackAmountOverride, bAllStackable, ItemLookup);
	const FIntPoint Dimensions = GetItemDimensions(Manifest);
	const FInv_StackableFragment* Stackable = Manifest.GetFragmentOfType<FInv_StackableFragment>();
	const int32 MaxStackCount = Stackable ? Stackable->GetMaxStackCount() : 1;
	if (Result.Remainder <= 0 || Col <= 0 || Dimensions.X <= 0 || Dimensions.Y <= 0
		|| Dimensions.X > Col || MaxStackCount <= 0)
		return Result;

	// 尾部追加完整空白区域，保证剩余堆叠都能放下，无需逐行反复搜索。
	const int32 StacksNeeded = FMath::DivideAndRoundUp(Result.Remainder, MaxStackCount);
	const int32 StacksPerBand = Col / Dimensions.X;
	const int32 BandsNeeded = FMath::DivideAndRoundUp(StacksNeeded, StacksPerBand);
	GrowRows(BandsNeeded * Dimensions.Y);
	return Query(Manifest, StackAmountOverride, bAllStackable, ItemLookup);
}

FInv_SlotAvailabilityResult FInv_GridLayout::QueryInventory(const TArray<UInv_InventoryItem*>& Items,
	const UInv_InventoryGridAsset& Asset, const FInv_ItemManifest& Manifest, int32 StackAmountOverride, bool bAllStackable)
{
	FInv_SlotAvailabilityResult Result;
	const FInv_StackableFragment* Stack = Manifest.GetFragmentOfType<FInv_StackableFragment>();
	Result.bStackable = Stack != nullptr;
	Result.Remainder = Stack ? (StackAmountOverride == INDEX_NONE ? Stack->GetStackCount() : StackAmountOverride) : 1;
	// 第一次只取各网格已有堆叠，第二次只取空位，避免提前占用新格子。
	for (int32 Pass = bAllStackable && Stack ? 0 : 1; Pass < 2 && Result.Remainder > 0; ++Pass)
	{
		for (int32 SubGridIndex = 0; SubGridIndex < Asset.Grids.Num() && Result.Remainder > 0; ++SubGridIndex)
		{
			const UInv_InventoryGridConfig* Config = Asset.Grids[SubGridIndex];
			TArray<bool> Occupancy;
			BuildOccupancy(Items, Config->Col, Config->Row, Occupancy, SubGridIndex);
			TMap<int32, TWeakObjectPtr<UInv_InventoryItem>> ItemLookup;
			for (UInv_InventoryItem* Item : Items)
				if (Item->GetSubGridIndex() == SubGridIndex && Item->GetGridIndex() != INDEX_NONE)
					ItemLookup.Add(Item->GetGridIndex(), Item);
			FInv_GridLayout Layout;
			Layout.Setup(Config->Col, Config->Row, Occupancy);
			const FInv_SlotAvailabilityResult Space = Layout.Query(Manifest, Result.Remainder, Pass == 0, ItemLookup);
			for (FInv_SlotAvailability Slot : Space.SlotAvailabilities)
			{
				if (Pass == 0 && !Slot.bItemAtIndex)
					continue;
				Slot.SubGridIndex = SubGridIndex;
				Result.SlotAvailabilities.Add(Slot);
				Result.TotalRoomToFill += Slot.AmountToFill;
				Result.Remainder -= Slot.AmountToFill;
			}
		}
	}
	return Result;
}
