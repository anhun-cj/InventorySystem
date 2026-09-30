#include "Widget/Utils/Inv_WidgetUtils.h"

#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Widget.h"

FVector2D UInv_WidgetUtils::GetWidgetPosition(UWidget* Widget)
{
	const FGeometry Geometry = Widget->GetCachedGeometry();
	FVector2D PixelPosition;
	FVector2D ViewPortPosition;
	USlateBlueprintLibrary::LocalToViewport(Widget, Geometry, USlateBlueprintLibrary::GetLocalTopLeft(Geometry), PixelPosition, ViewPortPosition);
	return ViewPortPosition;
}

FVector2D UInv_WidgetUtils::GetWidgetSize(UWidget* Widget)
{
	const FGeometry Geometry = Widget->GetCachedGeometry();
	return Geometry.GetLocalSize();
}

bool UInv_WidgetUtils::IsWithinBounds(const FVector2D& BoundaryPos, const FVector2D& WidgetSize,
                                      const FVector2D& MousePos)
{
	return MousePos.X >= BoundaryPos.X && MousePos.X <= (BoundaryPos.X + WidgetSize.X) && MousePos.Y >= BoundaryPos.Y && MousePos.Y <= (BoundaryPos.Y + WidgetSize.Y);
}

FVector2D UInv_WidgetUtils::GetClampedWidgetPosition(const FVector2D& BoundaryPos, const FVector2D& WidgetSize,
	const FVector2D& MousePos)
{
	FVector2D ClampedPosition = MousePos;
	
	if (MousePos.X + WidgetSize.X > BoundaryPos.X)
	{
		ClampedPosition.X = BoundaryPos.X - WidgetSize.X;
	}
	
	if (MousePos.X < 0.0f)
	{
		ClampedPosition.X = 0.0f;
	}
	
	if (MousePos.Y + WidgetSize.Y > BoundaryPos.Y)
	{
		ClampedPosition.Y = BoundaryPos.Y - WidgetSize.Y;
	}
	
	if (MousePos.Y < 0.0f)
	{
		ClampedPosition.Y = 0.0f;
	}
	
	return ClampedPosition;
}

int32 UInv_WidgetUtils::GetIndexFromPosition(const FIntPoint& Position, const int32 Col)
{
	return Position.X + Position.Y * Col;
}

FIntPoint UInv_WidgetUtils::GetPositionFromIndex(const int32 Index, const int32 Col)
{
	return FIntPoint(Index % Col, Index / Col);
}
