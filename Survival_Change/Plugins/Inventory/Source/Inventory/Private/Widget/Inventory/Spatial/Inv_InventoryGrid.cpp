#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"

#include "Inventory.h"
#include "Widget/Inventory/GridSlot/Inv_GridSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Widget/Utils/Inv_WidgetUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/WrapBox.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "Items/Inv_InventoryItem.h"
#include "Items/Manifest/Inv_ItemManifest.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Net/UnrealNetwork.h"
#include "Widget/Inventory/HoverItem/Inv_HoverItem.h"
#include "Widget/Inventory/InventoryBase/Inv_InventoryBase.h"
#include "Widget/Inventory/SlottedItems/Inv_SlottedItem.h"
#include "Widget/ItemPopUp/Inv_ItemPopUp.h"

void UInv_InventoryGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	ConstructGrid();
}

void UInv_InventoryGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FVector2D CanvasPosition = UInv_WidgetUtils::GetWidgetPosition(CanvasPanel);
	FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	
	if (CursorExitedCanvas(CanvasPosition, UInv_WidgetUtils::GetWidgetSize(CanvasPanel), MousePosition))
	{
		return;
	}
	
	UpdateTileParameters(CanvasPosition, MousePosition);
}

void UInv_InventoryGrid::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UInv_InventoryGrid, CurrentQueryResult);
}

void UInv_InventoryGrid::UpdateTileParameters(const FVector2D& CanvasPosition, const FVector2D& MousePosition)
{
	if (!bMouseWithinCanvas)
		return;
	
	const FIntPoint HoveredTileCoordinates = CalculateHoveredCoordinates(CanvasPosition, MousePosition);
	LastTileParameters = TileParameters;
	TileParameters.TileCoordinates = HoveredTileCoordinates;
	TileParameters.TileIndex = UInv_WidgetUtils::GetIndexFromPosition(HoveredTileCoordinates, Col);
	TileParameters.TileQuadrant = CalculateTileQuadrant(CanvasPosition, MousePosition);
	
	OnTileParametersUpdated(TileParameters);
}

void UInv_InventoryGrid::OnTileParametersUpdated(const FInv_TileParameters& Parameters)
{
	if (!HoverItem.IsValid())
	{
		//跨容器界面(仓库)优先从共同的顶层菜单取 hover，保证两个网格看到同一个拖拽悬浮物；
		UInv_InventoryBase* HoverMenu = HoverSourceMenu.IsValid()
			? HoverSourceMenu.Get()
			: (InventoryComponent.IsValid() ? InventoryComponent->GetInventoryMenu() : nullptr);
		if (IsValid(HoverMenu))
			HoverItem = HoverMenu->GetHoverItem();
		if(!HoverItem.IsValid())
			return;
	}
	const FIntPoint Dimensions = HoverItem->GetGridDimensions();

	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinates, Dimensions, Parameters.TileQuadrant);
	
	ItemDropIndex = UInv_WidgetUtils::GetIndexFromPosition(StartingCoordinate, Col);

	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);
	
	HighLightSlots(ItemDropIndex, Dimensions);
}

FInv_SpaceQueryResult UInv_InventoryGrid::CheckHoverPosition(const FIntPoint& Position,
	const FIntPoint& Dimensions)
{
	FInv_SpaceQueryResult Result;
	
	if (!IsInGridBounds(UInv_WidgetUtils::GetIndexFromPosition(Position, Col), Dimensions))
		return Result;
	
	Result.bHasSpace = true;
	Result.bAllowItemDown = true;
	TSet<int32> OccupiedUpperLeftIndices;
	UInv_InventoryStatics::ForEch2D(GridSlots, UInv_WidgetUtils::GetIndexFromPosition(Position, Col), Dimensions, Col, [&, this](UInv_GridSlot* GridSlot)
	{
		/*if (GridSlot->GetInventoryItem().IsValid())
		{
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			Result.bHasSpace = false;
		}*/
		int32 Index = -1;
		if (GridSlot->GetInventoryItem().IsValid())
			Index = GridSlot->GetInventoryItem()->GetGridIndex();
		if (GridSlot->IsAvailable() || Index == HoverItem->GetInventoryItem()->GetGridIndex())
		{
			Result.Green.Add(true);
			return;
		}
		
		if (CheckHoverItemMoveItems(GridSlot, Position, Dimensions))
		{
			FIntPoint SourcePosition = UInv_WidgetUtils::GetPositionFromIndex(HoverItem->GetInventoryItem()->GetGridIndex(), HoverItem->GetCol());
			FIntPoint ItemPosition = CalculateMoveItemPosition(GridSlot, Position, SourcePosition);
			int32 Num = OccupiedUpperLeftIndices.Num();
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			const int32 MoveIndex = UInv_WidgetUtils::GetIndexFromPosition(ItemPosition, HoverItem->GetCol());
			if (Num != OccupiedUpperLeftIndices.Num())
			{
				Result.ValidItems.Add(GridSlot->GetInventoryItem());
				Result.ItemMoveIndices.Add(MoveIndex);
			}
			Result.Green.Add(true);
			Result.bHasSpace = false;
		}
		else
		{
			Result.bHasSpace = false;
			Result.bAllowItemDown = false;
			Result.Green.Add(false);
		}
	});
	
	for (int j = 0; j < Dimensions.Y; j++)
	{
		for (int i = 0; i < Dimensions.X; i++)
		{
			
		}
	}
	/*if (OccupiedUpperLeftIndices.Num() == 1)
	{
		const int32 Index = *OccupiedUpperLeftIndices.CreateConstIterator();
		Result.ValidItem = GridSlots[Index]->GetInventoryItem();
		Result.UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	}*/
	
	return Result;
}

bool UInv_InventoryGrid::CursorExitedCanvas(const FVector2D& BoundaryPos, const FVector2D& BoundarySize,
	const FVector2D& Location)
{
	bLastMouseWithinCanvas = bMouseWithinCanvas;
	bMouseWithinCanvas = UInv_WidgetUtils::IsWithinBounds(BoundaryPos, BoundarySize, Location);
	if (!bMouseWithinCanvas && bLastMouseWithinCanvas)
	{
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
		return true;
	}
	return false;
}

void UInv_InventoryGrid::HighLightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	if (!bMouseWithinCanvas)
		return;
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	UInv_InventoryStatics::ForEch2D(GridSlots, Index, Dimensions, Col, [this](UInv_GridSlot* GridSlot)
	{
		if (CurrentQueryResult.Green.IsEmpty())
			return;
		if (CurrentQueryResult.Green[0])
		{
			GridSlot->SetGreen();
		}
		else
		{
			GridSlot->SetRed();
		}
		CurrentQueryResult.Green.RemoveAt(0);
	});
	LastHighlightedIndex = Index;
	LastHighlightedDimensions = Dimensions;
}

void UInv_InventoryGrid::UnHighlightSlots(const int32 Index, const FIntPoint& Dimensions)
{
	UInv_InventoryStatics::ForEch2D(GridSlots, Index, Dimensions, Col, [](UInv_GridSlot* GridSlot)
	{
		GridSlot->SetNormal();
	});
}

FIntPoint UInv_InventoryGrid::CalculateStartingCoordinate(const FIntPoint& Coordinate, const FIntPoint& Dimensions,
                                                          const EInv_TileQuadrant Quadrant) const
{
	const int32 HasEventWidth = Dimensions.X % 2 == 0 ? 1 : 0;
	const int32 HasEventHeight = Dimensions.Y % 2 == 0 ? 1 : 0;
	
	FIntPoint StartingCoord;
	switch (Quadrant)
	{
		case EInv_TileQuadrant::TopLeft:
			StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5 * Dimensions.X);
			StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5 * Dimensions.Y);
			break;
		case EInv_TileQuadrant::TopRight:
			StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5 * Dimensions.X) + HasEventWidth;
			StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5 * Dimensions.Y);
			break;
		case EInv_TileQuadrant::BottomLeft:
			StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5 * Dimensions.X);
			StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5 * Dimensions.Y) + HasEventHeight;
			break;
		case EInv_TileQuadrant::BottomRight:
			StartingCoord.X = Coordinate.X - FMath::FloorToInt(0.5 * Dimensions.X) + HasEventWidth;
			StartingCoord.Y = Coordinate.Y - FMath::FloorToInt(0.5 * Dimensions.Y) + HasEventHeight;
			break;
		default:
			UE_LOG(LogInventory, Log, TEXT("无效的象限"));
		    return FIntPoint(-1,-1);
	}
	return StartingCoord;
}

FIntPoint UInv_InventoryGrid::CalculateHoveredCoordinates(const FVector2D& CanvasPosition,
                                                          const FVector2D& MousePosition) const
{
	return FIntPoint{
		static_cast<int32>(FMath::FloorToInt((MousePosition.X - CanvasPosition.X) / TileSize)), 
		static_cast<int32>(FMath::FloorToInt((MousePosition.Y - CanvasPosition.Y) / TileSize))
	};
}

EInv_TileQuadrant UInv_InventoryGrid::CalculateTileQuadrant(const FVector2D& CanvasPosition,
	const FVector2D& MousePosition) const
{
	//计数相对坐标
	const float TileLocalX = FMath::Fmod(MousePosition.X - CanvasPosition.X, TileSize);
	const float TileLocalY = FMath::Fmod(MousePosition.Y - CanvasPosition.Y, TileSize);
	//确定最终象限
	const bool bIsTop = TileLocalY < TileSize / 2; //小于一半在上半，大于一半在下半
	const bool bIsLeft = TileLocalX < TileSize / 2; //小于一半在左半，大于一半在右半
	
	EInv_TileQuadrant HoveredTileQuadrant = EInv_TileQuadrant::None;
	if (bIsLeft && bIsTop)
		HoveredTileQuadrant = EInv_TileQuadrant::TopLeft;
	else if (!bIsLeft && bIsTop)
		HoveredTileQuadrant = EInv_TileQuadrant::TopRight;
	else if (bIsLeft && !bIsTop)
		HoveredTileQuadrant = EInv_TileQuadrant::BottomLeft;
	else if (!bIsLeft && !bIsTop)
		HoveredTileQuadrant = EInv_TileQuadrant::BottomRight;
	return  HoveredTileQuadrant;
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(const UInv_ItemComponent* ItemComponent, const bool bAllStackable)
{
	return HasRoomForItem(ItemComponent->GetItemManifest(), -1, bAllStackable);
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(const UInv_InventoryItem* Item, const int32 StackAmountOverride, const bool bAllStackable)
{
	return HasRoomForItem(Item->GetItemManifest(), StackAmountOverride, bAllStackable);
}

FInv_SlotAvailabilityResult UInv_InventoryGrid::HasRoomForItem(const FInv_ItemManifest& Manifest, const int32 StackAmountOverride, const bool bAllStackable)
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
	TSet<UInv_InventoryItem*> SelectedItems;
	UInv_InventoryItem* Item = InventoryComponent->FindNoFillItem(Manifest, SelectedItems);
	do
	{
		if (!bAllStackable)
			break;
		if (IsValid(Item) && Result.bStackable)
		{
			const int32 AmountToFillSlot = DetermineFillAmountFotSlot(Result.bStackable, MaxStackCount, AmountToFill, GridSlots[Item->GetGridIndex()]);
			if (AmountToFillSlot > 0)
			{
				Result.SlotAvailabilities.Emplace(
				FInv_SlotAvailability({ Item->GetGridIndex(), AmountToFillSlot, true, Item}));
			}
			Result.TotalRoomToFill += AmountToFillSlot;
			AmountToFill -= AmountToFillSlot;
		}
		SelectedItems.Add(Item);
		Item = InventoryComponent->FindNoFillItem(Manifest, SelectedItems);
	}while (IsValid(Item));
	
	TSet<int32> CheckedIndices;
	for (const UInv_GridSlot* GridSlot : GridSlots)
	{
		if (AmountToFill == 0)
			break;
		//检查索引是否将会被使用
		if (IsIndexClaimed(CheckedIndices, GridSlot->GetTileIndex()))
			continue;
		//检查是否在背包范围内
		if (!IsInGridBounds(GridSlot->GetTileIndex(), GetItemDimensions(Manifest)))
			continue;
		if (!GridSlot->IsAvailable())
			continue;
		TSet<int32> SelectedIndices;
		if (!HasRoomAtIndex(GridSlot, GetItemDimensions(Manifest), CheckedIndices, SelectedIndices, Manifest.GetItemType(), MaxStackCount))
			continue;
		CheckedIndices.Append(SelectedIndices);
		//决定可以填充数量
		const int32 AmountToFillSlot = DetermineFillAmountFotSlot(Result.bStackable, MaxStackCount, AmountToFill, GridSlot);
		if (AmountToFillSlot == 0)
			continue;
		Result.TotalRoomToFill += AmountToFillSlot;
		//就地构造，避免临时对象的拷贝或移动
		Result.SlotAvailabilities.Emplace(
		FInv_SlotAvailability({HasValidItem(GridSlot) ? GridSlot->GetUpperLeftIndex() : GridSlot->GetTileIndex(),
		Result.bStackable ? AmountToFillSlot : 1, false, nullptr})
		);

		AmountToFill -= AmountToFillSlot;
		
		Result.Remainder = AmountToFill;
		if (AmountToFill == 0)
			return Result;
		
	}
	
	return Result;
}


FIntPoint UInv_InventoryGrid::GetItemDimensions(const FInv_ItemManifest& Manifest) const
{
	const FInv_GridFragment* GridFragment = Manifest.GetFragmentOfType<FInv_GridFragment>();
	return  GridFragment ? GridFragment->GetGridSize() : FIntPoint(1, 1);
}

bool UInv_InventoryGrid::IsIndexClaimed(const TSet<int32>& CheckedIndices, const int32 Index) const
{
	return CheckedIndices.Contains(Index);
}

bool UInv_InventoryGrid::HasRoomAtIndex(const UInv_GridSlot* GridSlot, const FIntPoint& Dimensions, const TSet<int32>& CheckedIndices,
		TSet<int32>& OutTentativelyClaimed,const FGameplayTag& ItemType, const int32 MaxStackCount)
{
	bool bHasRoom = true;
	//检测物品放置是否可以使用
	UInv_InventoryStatics::ForEch2D(GridSlots, GridSlot->GetTileIndex(), Dimensions, Col, [&](const UInv_GridSlot* SubGridSlot)
	{
		if (!CheckSlotContains(GridSlot, SubGridSlot, CheckedIndices, OutTentativelyClaimed, ItemType, MaxStackCount))
		{
			bHasRoom = false;
		}
	});

	return bHasRoom;
}

bool UInv_InventoryGrid::CheckSlotContains(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot, const TSet<int32>& CheckedIndices, TSet<int32>& OutTentativelyClaimed,
		const FGameplayTag& ItemType, const int32 MaxStackCount) const
{
	//网格被占用
	if (IsIndexClaimed(CheckedIndices, SubGridSlot->GetTileIndex()))
		return false;
	//物品无效
	if (!HasValidItem(SubGridSlot))
	{
		OutTentativelyClaimed.Add(SubGridSlot->GetTileIndex());
		return true;
	}
	//不是左上角索引
	/*if (!IsUpperLeftSlot(GridSlot, SubGridSlot))
		return false;*/
	
	return false;
}

bool UInv_InventoryGrid::HasValidItem(const UInv_GridSlot* GridSlot) const
{
	return GridSlot->GetInventoryItem().IsValid();
}

bool UInv_InventoryGrid::IsUpperLeftSlot(const UInv_GridSlot* GridSlot, const UInv_GridSlot* SubGridSlot) const
{
	return GridSlot->GetTileIndex() == SubGridSlot->GetUpperLeftIndex();
}

bool UInv_InventoryGrid::DoesItemMatch(const UInv_InventoryItem* SubItem, const FInv_ItemManifest& Source) const
{
	return IsValid(SubItem) && FInv_ItemManifest::AreSameItem(SubItem->GetItemManifest(), Source);
}

bool UInv_InventoryGrid::IsInGridBounds(const int32 StartIndex, const FIntPoint& ItemDimensions) const
{
	if (StartIndex < 0 || StartIndex >= GridSlots.Num())
		return false;
	const int32 EndCol = (StartIndex % Col) + ItemDimensions.X;
	const int32 EndRow = (StartIndex / Col) + ItemDimensions.Y;
	return EndCol <= Col && EndRow <= Row;
}

int32 UInv_InventoryGrid::DetermineFillAmountFotSlot(const bool bStackable, const int32 MaxStackCount,
	const int32 AmountToFill, const UInv_GridSlot* GridSlot) const
{
	const int32 RoomInSlot = MaxStackCount - GetStackAmount(GridSlot);
	return bStackable ? FMath::Min(AmountToFill, RoomInSlot) : 1;
}

int32 UInv_InventoryGrid::GetStackAmount(const UInv_GridSlot* GridSlot) const
{
	int32 CurrentSlotStackCount = GridSlot->GetStackCount();
	
	//多格物品的计数在第一个槽上（左上角），拿左上角计数
	const int32 UpperLeftIndex = GridSlot->GetUpperLeftIndex();
	if (UpperLeftIndex != INDEX_NONE)
	{
		UInv_GridSlot* UpperLeftSlot = GridSlots[UpperLeftIndex];
		CurrentSlotStackCount = UpperLeftSlot->GetStackCount();
	}
	return CurrentSlotStackCount;
}

void UInv_InventoryGrid::OnItemChange(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	const bool bStackable = StackableFragment != nullptr;
	const int32 StackCount = bStackable ? StackableFragment->GetStackCount() : 1;
	AddItemAtIndex(Item, Item->GetGridIndex(), bStackable, StackCount);
	UpdateGridSlots(Item, Item->GetGridIndex(), bStackable, StackCount);
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	ClearHoverItem();
}

void UInv_InventoryGrid::OnSlottedItemClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	//UE_LOG(LogTemp, Warning, TEXT("点击第 %d 网格索引物品"), GridIndex);
	check(GridSlots.IsValidIndex(GridIndex));
	UInv_InventoryItem* ClickedInventoryItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	
	/*if (!HoverItem.IsValid() && IsLeftClick(MouseEvent))
	{
		//移动插槽物品
		PickUp(ClickedInventoryItem, GridIndex);
		return;
	}*/
	
	if (IsRightClick(MouseEvent))
	{
		//CreateItemPopUp(GridIndex);
		return;
	}
}

void UInv_InventoryGrid::CreateItemPopUp(const int32 GridIndex)
{
	UInv_InventoryItem* InventoryItem = GridSlots[GridIndex]->GetInventoryItem().Get();
	if (!IsValid(InventoryItem))
		return;
	if (IsValid(GridSlots[GridIndex]->GetItemPopUp()))
		return;
	ItemPopUp = CreateWidget<UInv_ItemPopUp>(GetOwningPlayer(), ItemPopUpClass);
	OwningCanvasPanel->AddChild(ItemPopUp);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ItemPopUp);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	CanvasSlot->SetPosition(MousePosition - ItemPopUpOffset);
	CanvasSlot->SetSize(ItemPopUp->GetBoxSize());
	GridSlots[GridIndex]->SetItemPopUp(ItemPopUp);
	
	const int32 SliderMax = GridSlots[GridIndex]->GetStackCount() - 1;
	if (InventoryItem->IsStackable() && SliderMax > 0)
	{
		ItemPopUp->OnSplit.BindDynamic(this, &UInv_InventoryGrid::OnPopUpMenuSplit);
		ItemPopUp->SetSliderParameters(SliderMax, FMath::Max(1, GridSlots[GridIndex]->GetStackCount() / 2));
		
	}
	else
	{
		ItemPopUp->CollapseSplitButton();
	}
	ItemPopUp->OnDrop.BindDynamic(this, &UInv_InventoryGrid::OnPopUpMenuDrop);
	
	if (InventoryItem->IsConsumable())
	{
		ItemPopUp->OnConsume.BindDynamic(this, &UInv_InventoryGrid::OnPopUpMenuConsume);
	}
	else
	{
		ItemPopUp->CollapseConsumeButton();
	}
	
}

void UInv_InventoryGrid::PutHoverItemBack()
{
	if (!HoverItem.IsValid())
		return;
	/*FInv_SlotAvailabilityResult Result = HasRoomForItem(HoverItem->GetInventoryItem(), HoverItem->GetStackCount());
	Result.Item = HoverItem->GetInventoryItem();
	ChangeItemStack(Result);*/
	ClearHoverItem();
}

bool UInv_InventoryGrid::CheckHoverItemMoveItems(UInv_GridSlot* GridSlot, const FIntPoint& SourcePosition, const FIntPoint& SourceDimensions) const
{
	//计算源（悬停物品）的目标区域范围：Position 是左上角，占地范围是 [P, P + D - 1]
	const int32 SourceTop = SourcePosition.Y;
	const int32 SourceBottom = SourcePosition.Y + SourceDimensions.Y - 1;
	const int32 SourceLeft = SourcePosition.X;
	const int32 SourceRight = SourcePosition.X + SourceDimensions.X - 1;

	if (!IsValid(GridSlot))
		return false;
	UInv_InventoryItem* TargetItem = GridSlot->GetInventoryItem().Get();
	if (!IsValid(TargetItem))
		return false;
	const FInv_GridFragment* TargetGridFragment = TargetItem->GetItemManifest().GetFragmentOfType<FInv_GridFragment>();
	if (!TargetGridFragment)
		return false;

	const FIntPoint TargetPosition = UInv_WidgetUtils::GetPositionFromIndex(GridSlot->GetUpperLeftIndex(), Col);
	const FIntPoint TargetDimensions = TargetGridFragment->GetGridSize();

	const int32 TargetTop = TargetPosition.Y;
	const int32 TargetBottom = TargetPosition.Y + TargetDimensions.Y - 1;
	const int32 TargetLeft = TargetPosition.X;
	const int32 TargetRight = TargetPosition.X + TargetDimensions.X - 1;

	return TargetTop >= SourceTop && TargetBottom <= SourceBottom && TargetLeft >= SourceLeft && TargetRight <= SourceRight;
}

FIntPoint UInv_InventoryGrid::CalculateMoveItemPosition(UInv_GridSlot* GridSlot, const FVector2D& TargetPosition,
	const FVector2D& SourcePosition) const
{
	FIntPoint MoveItemPosition;

	const FVector2D ItemPosition = UInv_WidgetUtils::GetPositionFromIndex(GridSlot->GetUpperLeftIndex(), Col);
	
	MoveItemPosition.X = SourcePosition.X + ItemPosition.X - TargetPosition.X;
	MoveItemPosition.Y = SourcePosition.Y + ItemPosition.Y - TargetPosition.Y;


	const FIntPoint Dimensions = HoverItem->GetGridDimensions();
	const int32 SourceTop = SourcePosition.Y;
	const int32 SourceBottom = SourcePosition.Y + Dimensions.Y - 1;
	const int32 SourceLeft = SourcePosition.X;
	const int32 SourceRight = SourcePosition.X + Dimensions.X - 1;
	
	const FIntPoint TargetDimensions = GridSlot->GetInventoryItem()->GetItemManifest().GetFragmentOfType<FInv_GridFragment>()->GetGridSize();
	
	const int32 TargetTop = MoveItemPosition.Y;
	const int32 TargetBottom = MoveItemPosition.Y + TargetDimensions.Y - 1;
	const int32 TargetLeft = MoveItemPosition.X;
	const int32 TargetRight = MoveItemPosition.X + TargetDimensions.X - 1;
	
	if (!(TargetTop >= SourceTop && TargetBottom <= SourceBottom && TargetLeft >= SourceLeft && TargetRight <= SourceRight))
	{
		return MoveItemPosition;
	}
	if (Dimensions.X > abs(SourcePosition.X - TargetPosition.X))
	{
		MoveItemPosition.X += SourcePosition.X - TargetPosition.X;
		if (Dimensions.Y <= abs(SourcePosition.Y - TargetPosition.Y))
			MoveItemPosition.X -= SourcePosition.X - TargetPosition.X;
	}
	
	if (Dimensions.Y > abs(SourcePosition.Y - TargetPosition.Y))
	{
		MoveItemPosition.Y += SourcePosition.Y - TargetPosition.Y;
		if (Dimensions.X <= abs(SourcePosition.X - TargetPosition.X))
			MoveItemPosition.Y -= SourcePosition.Y - TargetPosition.Y;
	}
	
	return MoveItemPosition;
}

void UInv_InventoryGrid::DropItem()
{
	if (!HoverItem.IsValid() || !IsValid(HoverItem->GetInventoryItem()))
		return;
	//TODO:服务器上丢弃物品
	InventoryComponent->Server_DropItem(HoverItem->GetInventoryItem());
	ClearHoverItem();
	ShowCursor();
}

bool UInv_InventoryGrid::HasHoveredItem() const
{
	return HoverItem.IsValid();
}

UInv_HoverItem* UInv_InventoryGrid::GetHoverItem() const
{
	return HoverItem.Get();
}

bool UInv_InventoryGrid::IsRightClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;
}

bool UInv_InventoryGrid::IsLeftClick(const FPointerEvent& MouseEvent) const
{
	return MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton;
}

void UInv_InventoryGrid::PickUp(UInv_InventoryItem* ClickedInventoryItem, int32 GridIndex)
{
	AssignHoverItem(ClickedInventoryItem, GridIndex, GridIndex);
	
	RemoveItemFormGrid(ClickedInventoryItem, GridIndex);
}

void UInv_InventoryGrid::AssignHoverItem(UInv_InventoryItem* InventoryItem, int32 GridIndex, int32 PreviousGridIndex)
{
	AssignHoverItem(InventoryItem);
	
	HoverItem->SetPreviousGridIndex(PreviousGridIndex);
	HoverItem->UpDateStackCount(InventoryItem->IsStackable() ? GridSlots[GridIndex]->GetStackCount() : 0);
}

void UInv_InventoryGrid::RemoveItemFormGrid(UInv_InventoryItem* InventoryItem, int32 GridIndex)
{
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(InventoryItem, FragmentTags::GridFragment);
	if (!GridFragment)
		return;
	UInv_InventoryStatics::ForEch2D(GridSlots, GridIndex, GridFragment->GetGridSize(), Col, [](UInv_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
	});
	
	if (SlottedItems.Contains(GridIndex))
	{
		TObjectPtr<UInv_SlottedItem> FoundSlottedItem;
		SlottedItems.RemoveAndCopyValue(GridIndex, FoundSlottedItem);
		FoundSlottedItem->RemoveFromParent();
	}
}

void UInv_InventoryGrid::AssignHoverItem(UInv_InventoryItem* InventoryItem)
{
	if (!HoverItem.IsValid())
	{
		HoverItem = CreateWidget<UInv_HoverItem>(GetOwningPlayer(), HoverItemClass);
	}
	//获取物品的网格片段，用于判断需要多少网格
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(InventoryItem, FragmentTags::GridFragment);
	//获取物品的图像片段，用于显示物品
	const FInv_ImageFragment* ImageFragment = GetFragment<FInv_ImageFragment>(InventoryItem, FragmentTags::IconFragment);
	if (!ImageFragment || !GridFragment)
		return;
	const FVector2D DrawSize = GetDrawSize(GridFragment);
	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(ImageFragment->GetIcon());
	IconBrush.DrawAs = ESlateBrushDrawType::Image;
	IconBrush.ImageSize = DrawSize * UWidgetLayoutLibrary::GetViewportScale(this);
	HoverItem->SetImageBrush(IconBrush);
	HoverItem->SetGridDimensions(GridFragment->GetGridSize());
	HoverItem->SetInventoryItem(InventoryItem);
	HoverItem->SetIsStackable(InventoryItem->IsStackable());
	
	GetOwningPlayer()->SetMouseCursorWidget(EMouseCursor::Default, HoverItem.Get());
}

void UInv_InventoryGrid::OnHide()
{
	PutHoverItemBack();
}

void UInv_InventoryGrid::SetHoverItem(UInv_HoverItem* Item)
{
	if (HoverItem.IsValid())
		UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
	HoverItem = Item;
}

void UInv_InventoryGrid::UnHighlightSlots()
{
	UnHighlightSlots(LastHighlightedIndex, LastHighlightedDimensions);
}

void UInv_InventoryGrid::BindInventoryEvent(UInv_InventoryComponent* Inventory)
{
	InventoryComponent = Inventory;
	if (!InventoryComponent.IsValid())
		return;
	InventoryComponent->OnItemAdded.AddDynamic(this, &UInv_InventoryGrid::AddItem);
	InventoryComponent->OnItemChanged.AddDynamic(this, &UInv_InventoryGrid::OnItemChange);
	InventoryComponent->OnItemRemoved.AddDynamic(this, &UInv_InventoryGrid::RemoveItem);
	InventoryComponent->OnRemoveSlottedItem.AddDynamic(this, &UInv_InventoryGrid::RemoveItemFormGrid);
}

void UInv_InventoryGrid::UnBindInventoryEvent()
{
	if (!InventoryComponent.IsValid())
		return;
	InventoryComponent->OnItemAdded.RemoveDynamic(this, &UInv_InventoryGrid::AddItem);
	InventoryComponent->OnItemChanged.RemoveDynamic(this, &UInv_InventoryGrid::OnItemChange);
	InventoryComponent->OnItemRemoved.RemoveDynamic(this, &UInv_InventoryGrid::RemoveItem);
	InventoryComponent->OnRemoveSlottedItem.RemoveDynamic(this, &UInv_InventoryGrid::RemoveItemFormGrid);
}

void UInv_InventoryGrid::SetHoverSourceMenu(UInv_InventoryBase* InMenu)
{
	HoverSourceMenu = InMenu;
}

void UInv_InventoryGrid::RemoveItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	RemoveItemFormGrid(Item, Item->GetGridIndex());
}

void UInv_InventoryGrid::AddItem(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	//UE_LOG(LogTemp, Warning, TEXT("背包添加了一个物品"));
	//UI显示添加物品的图片，并加入背包
	AddItemToIndices(Item);
}

void UInv_InventoryGrid::InitInventory()
{
	const FInv_InventoryFastArray& InventoryFastArray = InventoryComponent->GetInventoryList();
	
	for (auto& GridSlot : GridSlots)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetAvailable(true);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetNormal();
	}
	
	for (auto& SlottedItem : SlottedItems)
	{
		SlottedItem.Value->RemoveFromParent();
	}
	
	SlottedItems.Empty();
	for (auto& Item : InventoryFastArray.GetAllItems())
	{
		AddItem(Item.GetItem());
	}
}

void UInv_InventoryGrid::ClearInventory()
{
	const FInv_InventoryFastArray& InventoryFastArray = InventoryComponent->GetInventoryList();
	
	for (auto& GridSlot : GridSlots)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetAvailable(true);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetNormal();
	}
	
	for (auto& SlottedItem : SlottedItems)
	{
		SlottedItem.Value->RemoveFromParent();
	}
	
	SlottedItems.Empty();
}

void UInv_InventoryGrid::AddItemToIndices(UInv_InventoryItem* Item)
{
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	bool bStackable = StackableFragment ? true : false;
	
	AddItemAtIndex(Item, Item->GetGridIndex(), bStackable, StackableFragment ? StackableFragment->GetStackCount() : 1);
	UpdateGridSlots(Item, Item->GetGridIndex(), bStackable, StackableFragment ? StackableFragment->GetStackCount() : 1);
}

void UInv_InventoryGrid::AddItemAtIndex(UInv_InventoryItem* Item, const int32 Index, const bool bStackable,
	const int32 StackAmount)
{
	//获取物品的网格片段，用于判断需要多少网格
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	//获取物品的图像片段，用于显示物品
	const FInv_ImageFragment* ImageFragment = GetFragment<FInv_ImageFragment>(Item, FragmentTags::IconFragment);
	if (!ImageFragment || !GridFragment)
		return;
	UInv_SlottedItem* SlottedItem = CreateSlottedItem(Item, bStackable, StackAmount, GridFragment, ImageFragment, Index);
	AddSlottedItemToCanvas(Index, GridFragment, SlottedItem);
	SlottedItem->SetInventoryGrid(this);
	SlottedItems.Add(Index, SlottedItem);
}

UInv_SlottedItem* UInv_InventoryGrid::CreateSlottedItem(UInv_InventoryItem* Item, const bool bStackable,
	const int32 StackAmount, const FInv_GridFragment* GridFragment, const FInv_ImageFragment* ImageFragment, const int32 Index)
{
	UInv_SlottedItem* SlottedItem = CreateWidget<UInv_SlottedItem>(GetOwningPlayer(), SlottedItemClass);
	SlottedItem->SetInventoryItem(Item);
	SetSlottedItem(SlottedItem, GridFragment, ImageFragment);
	SlottedItem->SetGridIndex(Index);
	SlottedItem->SetIsStackable(bStackable);
	const int32 StackUpDateAmount = bStackable ? StackAmount : 0;
	SlottedItem->UpDateStackCount(StackUpDateAmount);
	
	SlottedItem->OnSlottedItemClicked.AddDynamic(this, &UInv_InventoryGrid::OnSlottedItemClicked);
	
	return SlottedItem;
}

void UInv_InventoryGrid::SetSlottedItem(const UInv_SlottedItem* SlottedItem, const FInv_GridFragment* GridFragment,
                                        const FInv_ImageFragment* ImageFragment) const
{
	FSlateBrush Brush;
	Brush.SetResourceObject(ImageFragment->GetIcon());
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.ImageSize = GetDrawSize(GridFragment);
	SlottedItem->SetImageBrush(Brush);
}

void UInv_InventoryGrid::AddSlottedItemToCanvas(const int32 Index, const FInv_GridFragment* GridFragment,
	 UInv_SlottedItem* SlottedItem) const
{
	CanvasPanel->AddChild(SlottedItem);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(SlottedItem);
	CanvasSlot->SetSize(GetDrawSize(GridFragment));
	const FVector2D DrawPos = UInv_WidgetUtils::GetPositionFromIndex(Index, Col) * TileSize;
	const FVector2D DrawPosWithPadding = DrawPos + FVector2D(GridFragment->GetGridPadding());
	CanvasSlot->SetPosition(DrawPosWithPadding);
}

void UInv_InventoryGrid::UpdateGridSlots(UInv_InventoryItem* Item, int32 Index, bool bStackableItem, const int32 StackAmount)
{
	if (!IsValid (Item) || !GridSlots.IsValidIndex (Index))
		return ;
	
	if (bStackableItem)
	{
		GridSlots[Index]->SetStackCount(StackAmount);
	}
	
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1,1);
	
	UInv_InventoryStatics::ForEch2D(GridSlots, Index, Dimensions, Col,[&](UInv_GridSlot* GridSlot)
	{
		GridSlot->SetInventoryItem(Item);
		GridSlot->SetUpperLeftIndex(Index);
		GridSlot->SetAvailable(false);
		GridSlot->SetOccupiedTexture();
		GridSlot->SetNormal();
	} );
}



FVector2D UInv_InventoryGrid::GetDrawSize(const FInv_GridFragment* GridFragment) const
{
	const float IconTileWidth = TileSize - GridFragment->GetGridPadding() * 2;
	return GridFragment->GetGridSize() * IconTileWidth;
}

void UInv_InventoryGrid::ConstructGrid()
{
	GridSlots.Reserve(Row * Col);

	for (int32 j = 0; j < Row; ++j)
	{
		for (int32 i = 0; i < Col; ++i)
		{
			UInv_GridSlot* GridSlot = CreateWidget<UInv_GridSlot>(this, GridSlotClass);
			//CanvasPanel->AddChild(GridSlot);

			const FIntPoint TilePosition(i, j);

			GridSlot->SetTileIndex(UInv_WidgetUtils::GetIndexFromPosition(TilePosition, Col));
			GridSlot->SetInventoryGrid(this);
			GridSlot->SetNormal();
			GridSlot->SetUnoccupiedTexture();
			/*UCanvasPanelSlot* GridCPS = UWidgetLayoutLibrary::SlotAsCanvasSlot(GridSlot);
			GridCPS->SetSize(FVector2D(TileSize));
			GridCPS->SetPosition(TilePosition * TileSize);*/
			GridSlots.Add(GridSlot);
			
			WrapBox_GridSlots->AddChild(GridSlot);
			GridSlot->GridSlotClicked.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotClicked);
			GridSlot->GridSlotHoverer.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotHovered);
			GridSlot->GridSlotUnhovered.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotUnhovered);
		}
	}
}

void UInv_InventoryGrid::ConstructGridSlots()
{
	for (int32 i = 0; i < Col; ++i)
	{
		UInv_GridSlot* GridSlot = CreateWidget<UInv_GridSlot>(this, GridSlotClass);
		GridSlot->SetTileIndex(GridSlots.Num());
		GridSlot->SetInventoryGrid(this);
		GridSlot->SetNormal();
		GridSlot->SetUnoccupiedTexture();
		GridSlots.Add(GridSlot);
			
		WrapBox_GridSlots->AddChild(GridSlot);
		GridSlot->GridSlotClicked.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotClicked);
		GridSlot->GridSlotHoverer.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotHovered);
		GridSlot->GridSlotUnhovered.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotUnhovered);
	}
	Row += 1;
}

void UInv_InventoryGrid::OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (!HoverItem.IsValid())
		return;
	if (!GridSlots.IsValidIndex(ItemDropIndex))
		return;
	
	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		OnSlottedItemClicked(CurrentQueryResult.UpperLeftIndex, MouseEvent);
		return;
	}
	if (!IsInGridBounds(ItemDropIndex, HoverItem->GetGridDimensions()))
		return;
	
	UInv_GridSlot* GridSlot = GridSlots[ItemDropIndex];
	if (CurrentQueryResult.bHasSpace)
	{
		//将物品放置在这个位置
		PutDownOnIndex(ItemDropIndex);
		return;
	}
}

void UInv_InventoryGrid::PutDownOnIndex(const int32 Index)
{
	AddItemAtIndex(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	UpdateGridSlots(HoverItem->GetInventoryItem(), Index, HoverItem->IsStackable(), HoverItem->GetStackCount());
	ClearHoverItem();
}

void UInv_InventoryGrid::ClearHoverItem()
{
	if (!HoverItem.IsValid())
		return;
	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->SetIsStackable(false);
	HoverItem->UpDateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());
	
	HoverItem->RemoveFromParent();
	HoverItem = nullptr;
	
	ShowCursor();
}

UUserWidget* UInv_InventoryGrid::GetVisibleCurUserWidget()
{
	if (!IsValid(GetOwningPlayer()))
		return nullptr;
	
	if (!IsValid(VisibleCursorWidget))
	{
		VisibleCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), VisibleCursorWidgetClass);
	}
	return VisibleCursorWidget;
}

UUserWidget* UInv_InventoryGrid::GetHiddenCurUserWidget()
{
	if (!IsValid(GetOwningPlayer()))
		return nullptr;
	
	if (!IsValid(HiddenCursorWidget))
	{
		HiddenCursorWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), HiddenCursorWidgetClass);
	}
	return HiddenCursorWidget;
}

bool UInv_InventoryGrid::IsSameStackable(UInv_InventoryItem* ClickedInventoryItem) const
{
	const bool bIsSameInstance = ClickedInventoryItem == HoverItem->GetInventoryItem();
	const bool bIsStackable = ClickedInventoryItem->IsStackable();
	const bool bIsSameItemKind = FInv_ItemManifest::AreSameItem(
		ClickedInventoryItem->GetItemManifest(), HoverItem->GetInventoryItem()->GetItemManifest());
	return bIsSameInstance && bIsStackable && bIsSameItemKind;
}

void UInv_InventoryGrid::SwapWithHoverItem(UInv_InventoryItem* ClickedInventoryItem, const int32 GridIndex)
{
	if (!HoverItem.IsValid())
		return;
	UInv_InventoryItem* TempInventoryItem = HoverItem->GetInventoryItem();
	const int32 TempStackCount = HoverItem->GetStackCount();
	const bool bTempIsStackable = HoverItem->IsStackable();
	
	AssignHoverItem(ClickedInventoryItem, GridIndex, HoverItem->GetPreviousGridIndex());
	RemoveItemFormGrid(ClickedInventoryItem, GridIndex);
	AddItemAtIndex(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
	UpdateGridSlots(TempInventoryItem, ItemDropIndex, bTempIsStackable, TempStackCount);
}

bool UInv_InventoryGrid::ShouldSwapStackCount(const int32 RoomInClickedSlot, const int32 HoveredStackCount,
	const int32 MaxStackCount) const
{
	return RoomInClickedSlot == 0 && HoveredStackCount < MaxStackCount;
}

void UInv_InventoryGrid::SwapStackCounts(const int32 ClickedStackCount, const int32 HoveredStackCount,
	const int32 Index)
{
	UInv_GridSlot* GridSlot = GridSlots[Index];
	GridSlot->SetStackCount(HoveredStackCount);
	
	UInv_SlottedItem* SlottedItem = SlottedItems.FindChecked(Index);
	SlottedItem->UpDateStackCount(HoveredStackCount);
	
	HoverItem->UpDateStackCount(ClickedStackCount);
}

bool UInv_InventoryGrid::ShouldConsumeHoverItemStacks(const int32 HoveredStackCount,
	const int32 RoomInClickedSlot) const
{
	return RoomInClickedSlot >= HoveredStackCount;
}

void UInv_InventoryGrid::ConsumeHoverItemStacks(const int32 ClickedStackCount, const int32 HoveredStackCount,
	const int32 Index)
{
	const int32 AmountToTransfer = HoveredStackCount;
	const int32 NewClickedStackCount = ClickedStackCount + AmountToTransfer;
	
	GridSlots[Index]->SetStackCount(NewClickedStackCount);
	SlottedItems.FindChecked(Index)->UpDateStackCount(NewClickedStackCount);
	ClearHoverItem();
	ShowCursor();
	const FInv_GridFragment* GridFragment = GridSlots[Index]->GetInventoryItem()->GetItemManifest().GetFragmentOfType<FInv_GridFragment>();
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1,1);
	HighLightSlots(Index, Dimensions);
}

bool UInv_InventoryGrid::ShouldFillInStack(const int32 HoveredStackCount, const int32 RoomInClickedSlot) const
{
	return RoomInClickedSlot < HoveredStackCount;
}

void UInv_InventoryGrid::FillInStack(const int32 FillAmount, const int32 Remainder, const int32 Index)
{
	UInv_GridSlot* GridSlot = GridSlots[Index];
	const int32 NewStackCount = GridSlot->GetStackCount() + FillAmount;
	GridSlot->SetStackCount(NewStackCount);
	
	UInv_SlottedItem* ClickedSlottedItem = SlottedItems.FindChecked(Index);
	ClickedSlottedItem->UpDateStackCount(NewStackCount);
	
	HoverItem->UpDateStackCount(Remainder);
}

void UInv_InventoryGrid::ShowCursor()
{

}

void UInv_InventoryGrid::HideCursor()
{

}

void UInv_InventoryGrid::SetOwningCanvasPanel(UCanvasPanel* Canvas)
{
	OwningCanvasPanel = Canvas;
}

void UInv_InventoryGrid::OnGridSlotHovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (!HoverItem.IsValid())
		return;
	UInv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetOccupiedTexture();
	}
}

void UInv_InventoryGrid::OnGridSlotUnhovered(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (!HoverItem.IsValid())
		return;
	UInv_GridSlot* GridSlot = GridSlots[GridIndex];
	if (GridSlot->IsAvailable())
	{
		GridSlot->SetUnoccupiedTexture();
	}
}

void UInv_InventoryGrid::OnPopUpMenuSplit(int32 SplitAmount, int32 Index)
{
	UInv_InventoryItem* InventoryItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(InventoryItem))
		return;
	if (!InventoryItem->IsStackable())
		return;
	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	
	UInv_GridSlot* GridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = GridSlot->GetStackCount();
	const int32 NewStackCount = StackCount - SplitAmount;
	GridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpDateStackCount(NewStackCount);
	
	//TODO:点击拆分后直接在新加入一个物品进入背包
	AssignHoverItem(InventoryItem, UpperLeftIndex, UpperLeftIndex);
	HoverItem->UpDateStackCount(SplitAmount);
}

void UInv_InventoryGrid::OnPopUpMenuDrop(int32 Index)
{
	UInv_InventoryItem* InventoryItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(InventoryItem))
		return;
	PickUp(InventoryItem, Index);
	DropItem();
}

void UInv_InventoryGrid::OnPopUpMenuConsume(int32 Index)
{
	UInv_InventoryItem* InventoryItem = GridSlots[Index]->GetInventoryItem().Get();
	if (!IsValid(InventoryItem))
		return;
	const int32 UpperLeftIndex = GridSlots[Index]->GetUpperLeftIndex();
	UInv_GridSlot* GridSlot = GridSlots[UpperLeftIndex];
	const int32 NewStackCount = GridSlot->GetStackCount() - 1;
	GridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpDateStackCount(NewStackCount);
	
	InventoryComponent->Server_ConsumeItem(InventoryItem, Index);
	
	if (NewStackCount <= 0)
	{
		RemoveItemFormGrid(InventoryItem, Index);
	}
}

bool UInv_InventoryGrid::MatchesCategory(UInv_InventoryItem* Item) const
{
	return Item->GetItemManifest().GetItemCategory() == ItemCategory;
}


