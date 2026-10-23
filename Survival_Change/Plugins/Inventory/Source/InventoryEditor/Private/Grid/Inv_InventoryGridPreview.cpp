#include "Grid/Inv_InventoryGridPreview.h"

#include "Input/Reply.h"
#include "InventoryManagement/Grid/Inv_InventoryGridAsset.h"
#include "Rendering/DrawElements.h"

void SInv_InventoryGridPreview::Construct(const FArguments& InArgs)
{
	GridAsset = InArgs._GridAsset;
	SelectedGrid = InArgs._SelectedGrid;
	OnGridSelected = InArgs._OnGridSelected;
}

FVector2D SInv_InventoryGridPreview::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	FVector2D Size{0, 0};
	for (const UInv_InventoryGridConfig* Grid : GridAsset->Grids)
	{
		Size.X = FMath::Max(Size.X, Grid->PositionOffset.X + Grid->Col * GridAsset->TileSize);
		Size.Y = FMath::Max(Size.Y, Grid->PositionOffset.Y + Grid->Row * GridAsset->TileSize);
	}
	return Size;
}

int32 SInv_InventoryGridPreview::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	for (int32 i = 0; i < GridAsset->Grids.Num(); ++i)
	{
		const UInv_InventoryGridConfig* Grid = GridAsset->Grids[i];
		const FVector2D Position = Grid->PositionOffset;
		const float TileSize = GridAsset->TileSize;
		const FLinearColor Color = (SelectedGrid.Get() == i ? FLinearColor::Yellow : GridAsset->GridLineColor)
			* InWidgetStyle.GetColorAndOpacityTint();
		for (int32 x = 0; x <= Grid->Col; ++x)
		{
			const TArray<FVector2D> Points{Position + FVector2D(x * TileSize, 0),
				Position + FVector2D(x * TileSize, Grid->Row * TileSize)};
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + i, AllottedGeometry.ToPaintGeometry(),
				Points, ESlateDrawEffect::None, Color, true, GridAsset->GridLineThickness);
		}
		for (int32 y = 0; y <= Grid->Row; ++y)
		{
			const TArray<FVector2D> Points{Position + FVector2D(0, y * TileSize),
				Position + FVector2D(Grid->Col * TileSize, y * TileSize)};
			FSlateDrawElement::MakeLines(OutDrawElements, LayerId + i, AllottedGeometry.ToPaintGeometry(),
				Points, ESlateDrawEffect::None, Color, true, GridAsset->GridLineThickness);
		}
	}
	return LayerId + GridAsset->Grids.Num();
}

FReply SInv_InventoryGridPreview::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		return FReply::Unhandled();
	const FVector2D Position = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	for (int32 i = GridAsset->Grids.Num() - 1; i >= 0; --i)
	{
		const UInv_InventoryGridConfig* Grid = GridAsset->Grids[i];
		const FVector2D End = Grid->PositionOffset + FVector2D(Grid->Col, Grid->Row) * GridAsset->TileSize;
		if (FSlateRect(Grid->PositionOffset.X, Grid->PositionOffset.Y, End.X, End.Y).ContainsPoint(Position))
		{
			OnGridSelected.Execute(i);
			return FReply::Handled();
		}
	}
	return FReply::Unhandled();
}
