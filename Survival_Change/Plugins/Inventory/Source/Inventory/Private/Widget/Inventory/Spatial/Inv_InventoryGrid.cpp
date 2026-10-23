#include "Widget/Inventory/Spatial/Inv_InventoryGrid.h"

#include "Inventory.h"
#include "Widget/Inventory/GridSlot/Inv_GridSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Widget/Utils/Inv_WidgetUtils.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WrapBox.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Layout/Children.h"
#include "Widgets/SWidget.h"
#include "GameFramework/PlayerController.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "InventoryManagement/Grid/Inv_GridLayout.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
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
#include "Widget/Inventory/Spatial/Inv_SpatialInventory.h"
#include "Widget/ItemPopUp/Inv_ItemPopUp.h"

void UInv_InventoryGrid::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ScrollBox->OnScrollBarVisibilityChanged.AddUniqueDynamic(this, &UInv_InventoryGrid::OnScrollBarVisibilityChanged);
}

void UInv_InventoryGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!SubGrids.IsEmpty())
		return;
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
		if (Cast<UInv_FindItemComponent>(InventoryComponent.Get()) && IsValid(HoverItem->GetInventoryItem()))
			HasRoomForItem(HoverItem->GetInventoryItem(), -1, false);
	}
	// 鼠标指向容器图标时，显示的是内部存入结果，优先于外部网格的交换预览。
	if (Parameters.TileCoordinates.X >= 0 && Parameters.TileCoordinates.X < Col
		&& Parameters.TileCoordinates.Y >= 0 && Parameters.TileCoordinates.Y < Row)
	{
		UInv_GridSlot* TargetSlot = GridSlots[Parameters.TileIndex];
		UInv_InventoryItem* TargetContainer = TargetSlot->GetInventoryItem().Get();
		if (TargetContainer && TargetContainer->GridAsset)
		{
			UnHighlightSlots();
			ItemDropIndex = INDEX_NONE;
			CurrentQueryResult = FInv_SpaceQueryResult();
			
			for (UInv_InventoryItem* Parent = TargetContainer; Parent; Parent = Parent->ParentContainerItem.Get())
				if (Parent == HoverItem->GetInventoryItem())
					return;

			bool bCanStore = false;
			// 容器内部物品拖回自己的容器图标不发生转移，显示不可存入。
			if (HoverItem->GetGrid()->ContainerItem != TargetContainer)
			{
				UInv_InventoryComponent* GroundInventory = TargetContainer->GroundInventory.Get();
				// 地面容器读真实组件，已拾取容器读物品内部数组；均按资产容量查询，不扩行。
				const UInv_InventoryGridAsset* Asset = GroundInventory ? GroundInventory->GridAsset.Get() : TargetContainer->GridAsset.Get();
				const FInv_InventoryFastArray& List = GroundInventory ? GroundInventory->GetInventoryList() : TargetContainer->InventoryList;
				TArray<UInv_InventoryItem*> Items;
				TMap<int32, TWeakObjectPtr<UInv_InventoryItem>> ItemLookup;
				for (const FInv_InventoryEntry& Entry : List.GetAllItems())
				{
					Items.Add(Entry.GetItem());
					ItemLookup.Add(Entry.GridIndex, Entry.GetItem());
				}
				bCanStore = FInv_GridLayout::QueryInventory(Items, *Asset, HoverItem->GetInventoryItem()->GetItemManifest()).TotalRoomToFill > 0;
			}
			const FIntPoint TargetDimensions = GetItemDimensions(TargetContainer->GetItemManifest());
			// 高亮目标容器的整个占格范围；允许部分堆叠也属于可以接收。
			CurrentQueryResult.Green.Init(bCanStore, TargetDimensions.X * TargetDimensions.Y);
			HighLightSlots(UInv_WidgetUtils::GetPositionFromIndex(TargetSlot->GetUpperLeftIndex(), Col), TargetDimensions);
			return;
		}
	}
	const FIntPoint Dimensions = HoverItem->GetGridDimensions();

	const FIntPoint StartingCoordinate = CalculateStartingCoordinate(Parameters.TileCoordinates, Dimensions, Parameters.TileQuadrant);
	
	ItemDropIndex = UInv_WidgetUtils::GetIndexFromPosition(StartingCoordinate, Col);

	CurrentQueryResult = CheckHoverPosition(StartingCoordinate, Dimensions);
	CurrentQueryResult.TargetSubGridIndex = SubGridIndex;
	
	HighLightSlots(StartingCoordinate, Dimensions);
}

FInv_SpaceQueryResult UInv_InventoryGrid::CheckHoverPosition(const FIntPoint& Position,
	const FIntPoint& Dimensions)
{
	FInv_SpaceQueryResult Result;
	for (UInv_InventoryItem* Parent = ContainerItem; Parent; Parent = Parent->ParentContainerItem.Get())
		if (Parent == HoverItem->GetInventoryItem())
			return Result;
	
	if (Position.X < 0 || Position.Y < 0
		|| Position.X + Dimensions.X > Col || Position.Y + Dimensions.Y > Row)
	{
		Result.bHasSpace = false;
		Result.bAllowItemDown = false;
		UInv_InventoryStatics::ForEach2D(GridSlots, Position, Dimensions, Col, [&Result](UInv_GridSlot*)
		{
			Result.Green.Add(false);
		});
		return Result;
	}
	
	Result.bHasSpace = true;
	Result.bAllowItemDown = true;
	TSet<int32> OccupiedUpperLeftIndices;
	UInv_InventoryStatics::ForEach2D(GridSlots, Position, Dimensions, Col, [&, this](UInv_GridSlot* GridSlot)
	{
		/*if (GridSlot->GetInventoryItem().IsValid())
		{
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			Result.bHasSpace = false;
		}*/
		// 不同库存里的物品可以有相同的格子编号，只有同一物品才能忽略占用。
		if (GridSlot->IsAvailable() || GridSlot->GetInventoryItem().Get() == HoverItem->GetInventoryItem())
		{
			Result.Green.Add(true);
			return;
		}
		
		if (CheckHoverItemMoveItems(GridSlot, Position, Dimensions))
		{
			const FIntPoint SourceTile = UInv_WidgetUtils::GetPositionFromIndex(HoverItem->GetPreviousGridIndex(), HoverItem->GetCol());
			FIntPoint ItemPosition = CalculateMoveItemPosition(GridSlot, Position, SourceTile);
			int32 Num = OccupiedUpperLeftIndices.Num();
			OccupiedUpperLeftIndices.Add(GridSlot->GetUpperLeftIndex());
			const int32 MoveIndex = UInv_WidgetUtils::GetIndexFromPosition(ItemPosition, HoverItem->GetCol());
			if (Num != OccupiedUpperLeftIndices.Num())
			{
				Result.ValidItems.Add(GridSlot->GetInventoryItem());
				Result.ItemMoveIndices.Add(MoveIndex);
			}
			if (Num != OccupiedUpperLeftIndices.Num())
			{
				if (HoverItem->GetPreviousGridIndex() == INDEX_NONE)
				{
					if (GridSlot->GetInventoryItem()->GetItemManifest().GetItemType().MatchesTag(HoverItem->EquipmentType))
					{
						Result.Green.Add(true);
					}
					else
					{
						Result.bAllowItemDown = false;
						Result.Green.Add(false);
					}
				}
				else
				{
					Result.Green.Add(true);
				}
			}
			else
			{
				Result.Green.Add(true);
			}
			Result.bHasSpace = false;
		}
		else
		{
			Result.bHasSpace = false;
			Result.bAllowItemDown = false;
			Result.Green.Add(false);
		}
	});
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
		UnHighlightSlots(LastHighlightedCoordinate, LastHighlightedDimensions);
		ItemDropIndex = INDEX_NONE;
		LastHighlightedCoordinate = FIntPoint(INDEX_NONE, INDEX_NONE);
		LastHighlightedDimensions = FIntPoint::ZeroValue;
		CurrentQueryResult = FInv_SpaceQueryResult();
		return true;
	}
	return false;
}

void UInv_InventoryGrid::HighLightSlots(const FIntPoint& StartCoordinate, const FIntPoint& Dimensions)
{
	if (!bMouseWithinCanvas)
		return;
	UnHighlightSlots(LastHighlightedCoordinate, LastHighlightedDimensions);
	UInv_InventoryStatics::ForEach2D(GridSlots, StartCoordinate, Dimensions, Col, [this](UInv_GridSlot* GridSlot)
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
	LastHighlightedCoordinate = StartCoordinate;
	LastHighlightedDimensions = Dimensions;
}

void UInv_InventoryGrid::UnHighlightSlots(const FIntPoint& StartCoordinate, const FIntPoint& Dimensions)
{
	UInv_InventoryStatics::ForEach2D(GridSlots, StartCoordinate, Dimensions, Col, [](UInv_GridSlot* GridSlot)
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
	if (!InventoryComponent.IsValid())
		return Result;
	if (!SubGrids.IsEmpty())
	{
		const FInv_StackableFragment* Stack = Manifest.GetFragmentOfType<FInv_StackableFragment>();
		Result.bStackable = Stack != nullptr;
		Result.Remainder = Stack ? (StackAmountOverride == INDEX_NONE ? Stack->GetStackCount() : StackAmountOverride) : 1;
		for (int32 Pass = bAllStackable && Stack ? 0 : 1; Pass < 2 && Result.Remainder > 0; ++Pass)
			for (UInv_InventoryGrid* Grid : SubGrids)
			{
				const FInv_SlotAvailabilityResult Space = Grid->HasRoomForItem(Manifest, Result.Remainder, Pass == 0);
				for (const FInv_SlotAvailability& Availability : Space.SlotAvailabilities)
				{
					if (Pass == 0 && !Availability.bItemAtIndex)
						continue;
					Result.SlotAvailabilities.Add(Availability);
					Result.TotalRoomToFill += Availability.AmountToFill;
					Result.Remainder -= Availability.AmountToFill;
				}
				if (Result.Remainder == 0)
					break;
			}
		return Result;
	}
	{
		TArray<bool> Occupancy;
		TMap<int32, TWeakObjectPtr<UInv_InventoryItem>> ItemLookup;
		for (const UInv_GridSlot* GridSlot : GridSlots)
		{
			Occupancy.Add(!GridSlot->IsAvailable());
			if (UInv_InventoryItem* Item = GridSlot->GetInventoryItem().Get())
				ItemLookup.Add(GridSlot->GetUpperLeftIndex(), Item);
		}
		FInv_GridLayout Layout;
		Layout.Setup(Col, Row, Occupancy);
		Result = Cast<UInv_FindItemComponent>(InventoryComponent.Get())
			? Layout.QueryWithGrowth(Manifest, StackAmountOverride, bAllStackable, ItemLookup)
			: Layout.Query(Manifest, StackAmountOverride, bAllStackable, ItemLookup);
		EnsureRows(Layout.GetRow());
		for (FInv_SlotAvailability& Availability : Result.SlotAvailabilities)
			Availability.SubGridIndex = SubGridIndex;
		return Result;
	}

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
	AddItem(Item);
}

void UInv_InventoryGrid::OnSlottedItemClicked(UInv_InventoryItem* Item)
{
	UInv_InventoryStatics::ItemUnhovered(GetOwningPlayer());
	
	CreateItemPopUp(Item);
}

void UInv_InventoryGrid::CreateItemPopUp(UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	
	if (!GridSlots.IsValidIndex(Item->GetGridIndex()))
		return;
	const bool bNearbyItem = Cast<UInv_FindItemComponent>(Item->GetInventoryComponent()) != nullptr;
	const FInv_StackableFragment* Stackable = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	const int32 StackCount = Stackable ? Stackable->GetStackCount() : 1;
	if (bNearbyItem && !Item->IsConsumable() && StackCount <= 1)
		return;
	
	CloseItemPopUp();
	ItemPopUp = CreateWidget<UInv_ItemPopUp>(GetOwningPlayer(), ItemPopUpClass);
	
	OwningCanvasPanel->AddChild(ItemPopUp);
	ItemPopUp->SetInventoryItem(Item);
	UCanvasPanelSlot* CanvasSlot = UWidgetLayoutLibrary::SlotAsCanvasSlot(ItemPopUp);
	const FVector2D MousePosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	CanvasSlot->SetPosition(MousePosition - ItemPopUpOffset);
	CanvasSlot->SetSize(ItemPopUp->GetBoxSize());
	GridSlots[Item->GetGridIndex()]->SetItemPopUp(ItemPopUp);
	
	const int32 SliderMax = StackCount - 1;
	if (Item->IsStackable() && SliderMax > 0)
	{
		ItemPopUp->OnSplit.BindDynamic(this, &UInv_InventoryGrid::OnPopUpMenuSplit);
		ItemPopUp->SetSliderParameters(SliderMax, FMath::Max(1, StackCount / 2));
		
	}
	else
	{
		ItemPopUp->CollapseSplitButton();
	}
	if (bNearbyItem)
	{
		ItemPopUp->CollapseDropButton();
	}
	else
	{
		ItemPopUp->OnDrop.BindDynamic(this, &UInv_InventoryGrid::OnPopUpMenuDrop);
	}
	
	if (Item->IsConsumable())
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

FIntPoint UInv_InventoryGrid::CalculateMoveItemPosition(const UInv_GridSlot* GridSlot, const FIntPoint& TargetPosition,
	const FIntPoint& SourcePosition) const
{
	FIntPoint ItemPosition = UInv_WidgetUtils::GetPositionFromIndex(GridSlot->GetUpperLeftIndex(), Col);
	
	FIntPoint MoveItemPosition = SourcePosition + ItemPosition - TargetPosition;
	// 两个容器的坐标属于不同网格，不存在几何重叠。
	if (HoverItem->GetGrid() != this)
		return MoveItemPosition;

	const FIntPoint Dimensions = HoverItem->GetGridDimensions();
	const int32 SourceTop = TargetPosition.Y;
	const int32 SourceBottom = TargetPosition.Y + Dimensions.Y - 1;
	const int32 SourceLeft = TargetPosition.X;
	const int32 SourceRight = TargetPosition.X + Dimensions.X - 1;
	
	const FIntPoint TargetDimensions = GridSlot->GetInventoryItem()->GetItemManifest().GetFragmentOfType<FInv_GridFragment>()->GetGridSize();
	
	const int32 TargetTop = SourcePosition.Y;
	const int32 TargetBottom = SourcePosition.Y + Dimensions.Y - 1;
	const int32 TargetLeft = SourcePosition.X;
	const int32 TargetRight = SourcePosition.X + Dimensions.X - 1;
	
	bool bOverlap = TargetLeft <= SourceRight 
			 && TargetRight >= SourceLeft 
			 && TargetTop <= SourceBottom 
			 && TargetBottom >= SourceTop;
	if (!bOverlap)
	{
		return MoveItemPosition;
	}
	
	if (SourcePosition.X == TargetPosition.X)
	{
		if (SourcePosition.Y < TargetPosition.Y)
		{
			ItemPosition.Y -= Dimensions.Y;
		}
		else
		{
			ItemPosition.Y += Dimensions.Y;
		}
		return ItemPosition;
	}
	
	if (SourcePosition.Y == TargetPosition.Y)
	{
		if (SourcePosition.X < TargetPosition.X)
		{
			ItemPosition.X -= Dimensions.X;
		}
		else
		{
			ItemPosition.X += Dimensions.X;
		}
		return ItemPosition;
	}
	
	const double OverlapLeft   = FMath::Max(SourceLeft, TargetLeft);
	const double OverlapRight  = FMath::Min(SourceRight, TargetRight);
	const double OverlapTop    = FMath::Max(SourceTop, TargetTop);
	const double OverlapBottom = FMath::Min(SourceBottom, TargetBottom);
	
	FVector2D OverLapCenter = FVector2D((OverlapLeft + OverlapRight) / 2.0, (OverlapTop + OverlapBottom) / 2.0);
	FVector2D MoveItemCenter = FVector2D(ItemPosition.X + (TargetDimensions.X - 1) / 2.0, ItemPosition.Y + (TargetDimensions.Y - 1) / 2.0);
	
	MoveItemCenter += ((OverLapCenter - MoveItemCenter) * 2);
	ItemPosition = FIntPoint(MoveItemCenter.X - (TargetDimensions.X - 1) / 2.0, MoveItemCenter.Y - (TargetDimensions.Y - 1) / 2.0);

	return ItemPosition;
}

void UInv_InventoryGrid::DropItem()
{
	if (!HoverItem.IsValid() || !IsValid(HoverItem->GetInventoryItem()))
		return;
	//TODO:服务器上丢弃物品
	InventoryComponent->Server_DropItem(HoverItem->GetInventoryItem());
	ClearHoverItem();
}

bool UInv_InventoryGrid::HasHoveredItem() const
{
	return GetHoverItem() != nullptr;
}

UInv_HoverItem* UInv_InventoryGrid::GetHoverItem() const
{
	if (HoverItem.IsValid())
		return HoverItem.Get();
	for (UInv_InventoryGrid* Grid : SubGrids)
		if (UInv_HoverItem* Item = Grid->GetHoverItem())
			return Item;
	return nullptr;
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
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->RemoveItemFormGrid(InventoryItem, GridIndex);
	if (!IsValid(InventoryItem))
		return;
	for (UInv_GridSlot* GridSlot : GridSlots)
	{
		if (GridSlot->GetInventoryItem().Get() != InventoryItem)
			continue;
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetAvailable(true);
		GridSlot->SetStackCount(0);
	}

	for (auto It = SlottedItems.CreateIterator(); It; ++It)
	{
		if (IsValid(It.Value()) && It.Value()->GetInventoryItem() == InventoryItem)
		{
			It.Value()->RemoveFromParent();
			It.RemoveCurrent();
		}
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
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->OnHide();
	PutHoverItemBack();
}

void UInv_InventoryGrid::SetHoverItem(UInv_HoverItem* Item)
{
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->SetHoverItem(Item);
	if (HoverItem.IsValid())
		UnHighlightSlots(LastHighlightedCoordinate, LastHighlightedDimensions);
	HoverItem = Item;
}

void UInv_InventoryGrid::UnHighlightSlots()
{
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->UnHighlightSlots();
	UnHighlightSlots(LastHighlightedCoordinate, LastHighlightedDimensions);
}

// 仅在滚动条实际显示时预留 20 宽度，隐藏后恢复纯网格宽度。
void UInv_InventoryGrid::OnScrollBarVisibilityChanged(const ESlateVisibility NewVisibility)
{
	SizeBox_GridFrame->SetWidthOverride((SubGridIndex == INDEX_NONE ? LayoutSize.X : Col * TileSize)
		+ (!ParentGrid.IsValid() && NewVisibility == ESlateVisibility::Visible ? 20.0f : 0.0f));
}

// 同一个网格既可绑定组件顶层库存，也可绑定物品内部库存；布局统一从对应资产读取。
void UInv_InventoryGrid::BindInventoryEvent(UInv_InventoryComponent* Inventory, UInv_InventoryItem* InContainerItem, int32 InSubGridIndex)
{
	UnBindInventoryEvent();
	InventoryComponent = Inventory;
	if (!InventoryComponent.IsValid())
		return;
	ContainerItem = InContainerItem;
	const UInv_InventoryGridAsset* GridAsset = ContainerItem ? ContainerItem->GridAsset.Get() : Inventory->GridAsset.Get();
	TileSize = GridAsset->TileSize;
	SubGridIndex = Cast<UInv_FindItemComponent>(Inventory) ? 0 : InSubGridIndex;
	if (SubGridIndex == INDEX_NONE)
	{
		// 布局坐标已包含编辑器间距；按资产原坐标摆放，整组统一滚动。
		LayoutSize = FVector2D::ZeroVector;
		for (int32 i = 0; i < GridAsset->Grids.Num(); ++i)
		{
			const UInv_InventoryGridConfig* Config = GridAsset->Grids[i];
			const FVector2D Size(Config->Col * TileSize, Config->Row * TileSize);
			LayoutSize.X = FMath::Max(LayoutSize.X, Config->PositionOffset.X + Size.X);
			LayoutSize.Y = FMath::Max(LayoutSize.Y, Config->PositionOffset.Y + Size.Y);
			UInv_InventoryGrid* Grid = CreateWidget<UInv_InventoryGrid>(GetOwningPlayer(), SubGridClass);
			Grid->ParentGrid = this;
			Grid->SetOwningCanvasPanel(OwningCanvasPanel.Get());
			Grid->SetHoverSourceMenu(HoverSourceMenu.Get());
			UCanvasPanelSlot* GridSlot = CanvasPanel->AddChildToCanvas(Grid);
			GridSlot->SetPosition(Config->PositionOffset);
			GridSlot->SetSize(Size);
			SubGrids.Add(Grid);
			Grid->BindInventoryEvent(Inventory, InContainerItem, i);
		}
		Col = FMath::CeilToInt(LayoutSize.X / TileSize);
		Row = FMath::CeilToInt(LayoutSize.Y / TileSize);
		USizeBox* LayoutBox = WidgetTree->ConstructWidget<USizeBox>();
		LayoutBox->SetWidthOverride(LayoutSize.X);
		LayoutBox->SetHeightOverride(LayoutSize.Y);
		WrapBox_GridSlots->SetExplicitWrapSize(true);
		WrapBox_GridSlots->SetWrapSize(LayoutSize.X);
		WrapBox_GridSlots->AddChild(LayoutBox);
		SizeBox_GridFrame->SetHeightOverride(FMath::Min(LayoutSize.Y, SizeBox_GridFrame->GetMaxDesiredHeight()));

	}
	else
	{
		Col = GridAsset->Grids[SubGridIndex]->Col;
		Row = GridAsset->Grids[SubGridIndex]->Row;
		TileSize = GridAsset->TileSize;
		SizeBox_GridFrame->SetHeightOverride(FMath::Min(Row * TileSize, SizeBox_GridFrame->GetMaxDesiredHeight()));

		WrapBox_GridSlots->SetExplicitWrapSize(true);
		WrapBox_GridSlots->SetWrapSize(Col * TileSize);
		ConstructGrid();
		if (ParentGrid.IsValid())
		{
			ScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);
			ScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Never);
			SizeBox_GridFrame->SetMaxDesiredHeight(Row * TileSize);
			SizeBox_GridFrame->SetHeightOverride(Row * TileSize);
			SizeBox_GridFrame->SetWidthOverride(Col * TileSize);
			InitInventory();
			return;
		}
	}

	TArray<TSharedRef<SWidget>> Widgets{ScrollBox->TakeWidget()};
	// 首次绑定先读取 Slate 滚动条的实际状态，避免只等状态事件而漏掉初始宽度。
	while (!Widgets.IsEmpty())
	{
		const TSharedRef<SWidget> Widget = Widgets.Pop();
		if (Widget->GetType() == FName(TEXT("SScrollBar")))
		{
			OnScrollBarVisibilityChanged(UWidget::ConvertRuntimeToSerializedVisibility(Widget->GetVisibility()));
			break;
		}
		FChildren* Children = Widget->GetChildren();
		for (int32 i = 0; i < Children->Num(); ++i)
			Widgets.Add(Children->GetChildAt(i));
	}
	if (ContainerItem)
	{
		ContainerItem->OnItemAdded.AddDynamic(this, &UInv_InventoryGrid::AddItem);
		ContainerItem->OnItemChanged.AddDynamic(this, &UInv_InventoryGrid::OnItemChange);
		ContainerItem->OnItemRemoved.AddDynamic(this, &UInv_InventoryGrid::RemoveItem);
		InitInventory();
		return;
	}
	Inventory->SetBoundInventoryGrid(this);
	if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(Inventory))
		NearbyInventory->SetInventoryGrid(this);
	const bool bPlayerBackpack = Cast<APlayerController>(Inventory->GetOwner())
		&& !Cast<UInv_FindItemComponent>(Inventory);
	InventoryComponent->OnItemAdded.AddDynamic(this, &UInv_InventoryGrid::AddItem);
	InventoryComponent->OnItemChanged.AddDynamic(this, &UInv_InventoryGrid::OnItemChange);
	InventoryComponent->OnItemRemoved.AddDynamic(this, &UInv_InventoryGrid::RemoveItem);
	InventoryComponent->OnRemoveSlottedItem.AddDynamic(this, &UInv_InventoryGrid::RemoveItemFormGrid);
	if (bPlayerBackpack)
		InitInventory();
}

// 解除旧库存通知并清空物品 UI、格子和高亮记录，供卸装或重新绑定使用。
void UInv_InventoryGrid::UnBindInventoryEvent()
{
	for (UInv_InventoryGrid* Grid : SubGrids)
	{
		Grid->UnBindInventoryEvent();
		Grid->RemoveFromParent();
	}
	SubGrids.Empty();
	if (ParentGrid.IsValid())
	{
		ContainerItem = nullptr;
		InventoryComponent.Reset();
		ClearInventory();
		WrapBox_GridSlots->ClearChildren();
		GridSlots.Empty();
		return;
	}
	if (ContainerItem)
	{
		ContainerItem->OnItemAdded.RemoveDynamic(this, &UInv_InventoryGrid::AddItem);
		ContainerItem->OnItemChanged.RemoveDynamic(this, &UInv_InventoryGrid::OnItemChange);
		ContainerItem->OnItemRemoved.RemoveDynamic(this, &UInv_InventoryGrid::RemoveItem);
	}
	else if (InventoryComponent.IsValid())
	{
		InventoryComponent->OnItemAdded.RemoveDynamic(this, &UInv_InventoryGrid::AddItem);
		InventoryComponent->OnItemChanged.RemoveDynamic(this, &UInv_InventoryGrid::OnItemChange);
		InventoryComponent->OnItemRemoved.RemoveDynamic(this, &UInv_InventoryGrid::RemoveItem);
		InventoryComponent->OnRemoveSlottedItem.RemoveDynamic(this, &UInv_InventoryGrid::RemoveItemFormGrid);
		if (UInv_FindItemComponent* NearbyInventory = Cast<UInv_FindItemComponent>(InventoryComponent.Get()))
			NearbyInventory->SetInventoryGrid(nullptr);
		InventoryComponent->SetBoundInventoryGrid(nullptr);
	}
	ContainerItem = nullptr;
	InventoryComponent.Reset();
	ClearInventory();
	WrapBox_GridSlots->ClearChildren();
	GridSlots.Empty();
	LastHighlightedCoordinate = FIntPoint(INDEX_NONE, INDEX_NONE);
	LastHighlightedDimensions = FIntPoint::ZeroValue;
}

void UInv_InventoryGrid::SetHoverSourceMenu(UInv_InventoryBase* InMenu)
{
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->SetHoverSourceMenu(InMenu);
	HoverSourceMenu = InMenu;
}

void UInv_InventoryGrid::AddItemToIndices(UInv_InventoryItem* Item, int32 GridIndex)
{
	const FInv_StackableFragment* StackableFragment = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	AddItemAtIndex(Item, GridIndex, Item->IsStackable(), StackableFragment ? StackableFragment->GetStackCount() : 1);
	UpdateGridSlots(Item, GridIndex, Item->IsStackable(), StackableFragment ? StackableFragment->GetStackCount() : 1);
}

void UInv_InventoryGrid::AddItem()
{
	UInv_SpatialInventory* Menu = Cast<UInv_SpatialInventory>(HoverSourceMenu.Get());
	if (!CurrentQueryResult.bHasSpace && !CurrentQueryResult.bAllowItemDown)
	{
		ClearHoverItem();
		return;
	}
	UInv_InventoryItem* Item = HoverItem->GetInventoryItem();
	UInv_InventoryGrid* SourceGrid = HoverItem->GetGrid();
	const int32 SourceIndex = HoverItem->GetPreviousGridIndex();
	const int32 DropIndex = ItemDropIndex;
	FInv_SpaceQueryResult Result = CurrentQueryResult;
	Result.TargetSubGridIndex = SubGridIndex;
	for (UInv_InventoryItem* Parent = ContainerItem; Parent; Parent = Parent->ParentContainerItem.Get())
		if (Parent == Item)
		{
			Menu->ClearHoverItem();
			return;
		}
	for (const TWeakObjectPtr<UInv_InventoryItem>& MovedItem : Result.ValidItems)
		if (!MovedItem.IsValid())
		{
			Menu->ClearHoverItem();
			return;
		}
	if (SourceGrid == this && SourceIndex == DropIndex)
	{
		Menu->ClearHoverItem();
		return;
	}
	UnHighlightSlots();
	SourceGrid->RemoveItem(Item);
	if (Result.bHasSpace)
		AddItemToIndices(Item, DropIndex);
	else
		SwapWithHoverItem(Result.ValidItems.Num() == 1 ? Result.ValidItems[0].Get() : nullptr, DropIndex);
	Menu->ClearHoverItem();
	GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>()->TransferItem(Item, Result, DropIndex,
		InventoryComponent.Get(), SourceGrid->GetInventoryComponent(), SourceIndex, ContainerItem, SourceGrid->ContainerItem, SourceGrid->SubGridIndex, SubGridIndex);
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
	RemoveItem(Item);
	if (!SubGrids.IsEmpty())
	{
		for (UInv_InventoryGrid* Grid : SubGrids)
			Grid->AddItemToIndices(Item);
		return;
	}
	AddItemToIndices(Item);
}

void UInv_InventoryGrid::InitInventory()
{
	if (!SubGrids.IsEmpty())
	{
		for (UInv_InventoryGrid* Grid : SubGrids)
			Grid->InitInventory();
		return;
	}
	const FInv_InventoryFastArray& InventoryFastArray = ContainerItem ? ContainerItem->InventoryList : InventoryComponent->GetInventoryList();
	
	for (auto& GridSlot : GridSlots)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetAvailable(true);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetNormal();
		GridSlot->SetStackCount(0);
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
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->ClearInventory();
	SetHoverItem(nullptr);
	CurrentQueryResult = FInv_SpaceQueryResult();
	ItemDropIndex = INDEX_NONE;
	CloseItemPopUp();
	
	for (auto& GridSlot : GridSlots)
	{
		GridSlot->SetInventoryItem(nullptr);
		GridSlot->SetAvailable(true);
		GridSlot->SetUpperLeftIndex(INDEX_NONE);
		GridSlot->SetUnoccupiedTexture();
		GridSlot->SetNormal();
		GridSlot->SetStackCount(0);
	}
	
	for (auto& SlottedItem : SlottedItems)
	{
		SlottedItem.Value->RemoveFromParent();
	}
	
	SlottedItems.Empty();
}

void UInv_InventoryGrid::CloseItemPopUp()
{
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->CloseItemPopUp();
	if (IsValid(ItemPopUp))
		ItemPopUp->RemoveFromParent();
	ItemPopUp = nullptr;
}

void UInv_InventoryGrid::AddItemToIndices(UInv_InventoryItem* Item)
{
	const FInv_InventoryFastArray& InventoryFastArray = ContainerItem ? ContainerItem->InventoryList : InventoryComponent->GetInventoryList();
	for (const FInv_InventoryEntry& Entry : InventoryFastArray.GetAllItems())
	{
		if (Entry.GetItem() != Item || Entry.GridIndex == INDEX_NONE || Entry.SubGridIndex != SubGridIndex)
			continue;
		AddItemAtIndex(Item, Entry.GridIndex, Item->IsStackable(), Entry.StackCount);
		UpdateGridSlots(Item, Entry.GridIndex, Item->IsStackable(), Entry.StackCount);
		return;
	}
}

void UInv_InventoryGrid::AddItemAtIndex(UInv_InventoryItem* Item, const int32 Index, const bool bStackable,
	const int32 StackAmount)
{
	if (TObjectPtr<UInv_SlottedItem>* Existing = SlottedItems.Find(Index))
	{
		if (IsValid(*Existing))
			(*Existing)->RemoveFromParent();
		SlottedItems.Remove(Index);
	}
	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
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
	if (!IsValid(Item) || Index < 0)
		return;

	const FInv_GridFragment* GridFragment = GetFragment<FInv_GridFragment>(Item, FragmentTags::GridFragment);
	const FIntPoint Dimensions = GridFragment ? GridFragment->GetGridSize() : FIntPoint(1,1);
	// 多格物品的底部也必须在扩展后的网格内。
	const int32 RequiredRows = Index / Col + Dimensions.Y;
	if (!EnsureRows(RequiredRows)
		|| !IsInGridBounds(Index, Dimensions))
		return;

	if (!GridSlots.IsValidIndex (Index))
		return ;
	
	if (bStackableItem)
	{
		GridSlots[Index]->SetStackCount(StackAmount);
	}
	
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
			
			USizeBox* GridSlotBox = WidgetTree->ConstructWidget<USizeBox>();
			GridSlotBox->SetWidthOverride(TileSize);
			GridSlotBox->SetHeightOverride(TileSize);
			GridSlotBox->AddChild(GridSlot);
			WrapBox_GridSlots->AddChild(GridSlotBox);
			GridSlot->GridSlotClicked.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotClicked);
			GridSlot->GridSlotHoverer.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotHovered);
			GridSlot->GridSlotUnhovered.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotUnhovered);
		}
	}
}

bool UInv_InventoryGrid::EnsureRows(int32 RequiredRows)
{
	if (RequiredRows < 0 || (!Cast<UInv_FindItemComponent>(InventoryComponent.Get()) && RequiredRows > Row))
		return false;
	while (Row < RequiredRows)
		ConstructGridSlots();
	return true;
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
			
		USizeBox* GridSlotBox = WidgetTree->ConstructWidget<USizeBox>();
		GridSlotBox->SetWidthOverride(TileSize);
		GridSlotBox->SetHeightOverride(TileSize);
		GridSlotBox->AddChild(GridSlot);
		WrapBox_GridSlots->AddChild(GridSlotBox);
		GridSlot->GridSlotClicked.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotClicked);
		GridSlot->GridSlotHoverer.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotHovered);
		GridSlot->GridSlotUnhovered.AddDynamic(this, &UInv_InventoryGrid::OnGridSlotUnhovered);
	}
	Row += 1;
	SizeBox_GridFrame->SetHeightOverride(FMath::Min(Row * TileSize, SizeBox_GridFrame->GetMaxDesiredHeight()));
}

void UInv_InventoryGrid::OnGridSlotClicked(int32 GridIndex, const FPointerEvent& MouseEvent)
{
	if (!HoverItem.IsValid())
		return;
	if (!GridSlots.IsValidIndex(ItemDropIndex))
		return;
	
	if (CurrentQueryResult.ValidItem.IsValid() && GridSlots.IsValidIndex(CurrentQueryResult.UpperLeftIndex))
	{
		//OnSlottedItemClicked(CurrentQueryResult.UpperLeftIndex, MouseEvent);
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
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->ClearHoverItem();
	UnHighlightSlots();
	if (!HoverItem.IsValid())
		return;
	HoverItem->SetInventoryItem(nullptr);
	HoverItem->SetPreviousGridIndex(INDEX_NONE);
	HoverItem->SetIsStackable(false);
	HoverItem->UpDateStackCount(0);
	HoverItem->SetImageBrush(FSlateNoResource());
	
	HoverItem->RemoveFromParent();
	HoverItem = nullptr;
	
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
	UInv_InventoryItem* Item = HoverItem->GetInventoryItem();
	UInv_InventoryGrid* SourceGrid = HoverItem->GetGrid();
	int32 TargetIndex = INDEX_NONE;
	if (ClickedInventoryItem)
		for (const auto& Pair : SlottedItems)
			if (Pair.Value->GetInventoryItem() == ClickedInventoryItem)
				TargetIndex = Pair.Key;
	const FInv_StackableFragment* Stack = ClickedInventoryItem
		? ClickedInventoryItem->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>() : nullptr;
	const int32 TargetCount = TargetIndex != INDEX_NONE ? GetStackAmount(GridSlots[TargetIndex]) : 0;
	const int32 Room = Stack ? Stack->GetMaxStackCount() - TargetCount : 0;
	if (TargetIndex != INDEX_NONE && Room > 0 && HoverItem->IsStackable()
		&& FInv_ItemManifest::AreSameItem(Item->GetItemManifest(), ClickedInventoryItem->GetItemManifest()))
	{
		const int32 Count = HoverItem->GetStackCount();
		if (ShouldConsumeHoverItemStacks(Count, Room))
			ConsumeHoverItemStacks(TargetCount, Count, TargetIndex);
		else
		{
			FillInStack(Room, Count - Room, TargetIndex);
			SourceGrid->AddItemAtIndex(Item, HoverItem->GetPreviousGridIndex(), true, Count - Room);
			SourceGrid->UpdateGridSlots(Item, HoverItem->GetPreviousGridIndex(), true, Count - Room);
		}
		return;
	}
	for (const TWeakObjectPtr<UInv_InventoryItem>& MovedItem : CurrentQueryResult.ValidItems)
		RemoveItem(MovedItem.Get());
	AddItemToIndices(Item, GridIndex);
	for (int32 i = 0; i < CurrentQueryResult.ValidItems.Num(); i++)
		SourceGrid->AddItemToIndices(CurrentQueryResult.ValidItems[i].Get(), CurrentQueryResult.ItemMoveIndices[i]);
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

void UInv_InventoryGrid::SetOwningCanvasPanel(UCanvasPanel* Canvas)
{
	for (UInv_InventoryGrid* Grid : SubGrids)
		Grid->SetOwningCanvasPanel(Canvas);
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

void UInv_InventoryGrid::OnPopUpMenuSplit(int32 SplitAmount, UInv_InventoryItem* Item)
{
	if (!IsValid(Item))
		return;
	const FInv_StackableFragment* Stackable = Item->GetItemManifest().GetFragmentOfType<FInv_StackableFragment>();
	if (!Stackable || SplitAmount <= 0 || SplitAmount >= Stackable->GetStackCount())
		return;

	FInv_SlotAvailabilityResult Result = GetSubGrid(INDEX_NONE)->HasRoomForItem(Item, SplitAmount, false);
	if (!Result.SlotAvailabilities.IsValidIndex(0) || Result.SlotAvailabilities[0].AmountToFill < SplitAmount)
		return;
	
	if (UInv_RequestComponent* RequestComponent = GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>())
	{
		RequestComponent->SplitItem(Item, SplitAmount, Result.SlotAvailabilities[0].Index, Result.SlotAvailabilities[0].SubGridIndex);
	}
	
	/*UInv_GridSlot* GridSlot = GridSlots[UpperLeftIndex];
	const int32 StackCount = GridSlot->GetStackCount();
	const int32 NewStackCount = StackCount - SplitAmount;
	GridSlot->SetStackCount(NewStackCount);
	SlottedItems.FindChecked(UpperLeftIndex)->UpDateStackCount(NewStackCount);
	
	//TODO:点击拆分后直接在新加入一个物品进入背包
	AssignHoverItem(InventoryItem, UpperLeftIndex, UpperLeftIndex);
	HoverItem->UpDateStackCount(SplitAmount);*/
}

void UInv_InventoryGrid::OnPopUpMenuDrop(UInv_InventoryItem* Item)
{
	if (!IsValid(Item) || Cast<UInv_FindItemComponent>(Item->GetInventoryComponent()))
		return;
	if (UInv_RequestComponent* RequestComponent = GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>())
	{
		RequestComponent->DropItem(Item);
	}
}

void UInv_InventoryGrid::OnPopUpMenuConsume(UInv_InventoryItem* Item)
{
	if (!IsValid(Item) || !Item->IsConsumable())
		return;
	if (UInv_RequestComponent* RequestComponent = GetOwningPlayer()->FindComponentByClass<UInv_RequestComponent>())
	{
		RequestComponent->ConsumeItem(Item);
	}
	
}

bool UInv_InventoryGrid::MatchesCategory(UInv_InventoryItem* Item) const
{
	return Item->GetItemManifest().GetItemCategory() == ItemCategory;
}



// INDEX_NONE 取得整组库存，其余编号取得实际子网格；附近单网格始终返回自身。
UInv_InventoryGrid* UInv_InventoryGrid::GetSubGrid(int32 Index) const
{
	if (ParentGrid.IsValid())
		return ParentGrid->GetSubGrid(Index);
	return Index == INDEX_NONE || SubGrids.IsEmpty() ? const_cast<UInv_InventoryGrid*>(this) : SubGrids[Index].Get();
}
