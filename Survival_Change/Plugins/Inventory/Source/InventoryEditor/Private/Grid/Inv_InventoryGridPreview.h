#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UInv_InventoryGridAsset;

DECLARE_DELEGATE_OneParam(FInvGridSelected, int32);

class SInv_InventoryGridPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SInv_InventoryGridPreview) {}
		SLATE_ARGUMENT(UInv_InventoryGridAsset*, GridAsset)
		SLATE_ATTRIBUTE(int32, SelectedGrid)
		SLATE_EVENT(FInvGridSelected, OnGridSelected)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	TWeakObjectPtr<UInv_InventoryGridAsset> GridAsset;
	TAttribute<int32> SelectedGrid;
	FInvGridSelected OnGridSelected;
};
