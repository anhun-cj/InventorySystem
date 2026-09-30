#include "Widget/Composite/Inv_Leaf_Image.h"

#include "Components/Image.h"
#include "Components/SizeBox.h"

void UInv_Leaf_Image::SetImage(UTexture2D* texture) const
{
	Image_Icon->SetBrushFromTexture(texture);
}

void UInv_Leaf_Image::SetBoxSize(const FVector2D& size) const
{
	SizeBox_Icon->SetWidthOverride(size.X);
	SizeBox_Icon->SetHeightOverride(size.Y);
}

void UInv_Leaf_Image::SetImageSize(const FVector2D& size) const
{
	Image_Icon->SetDesiredSizeOverride(size);
}

FVector2D UInv_Leaf_Image::GetImageSize() const
{
	return Image_Icon->GetDesiredSize();
}
