#include "Widget/Composite/Inv_Leaf_Text.h"

#include "Components/TextBlock.h"

void UInv_Leaf_Text::SetText(const FText& Text) const
{
	TextBlock->SetText(Text);
}

void UInv_Leaf_Text::NativePreConstruct()
{
	Super::NativePreConstruct();
	
	FSlateFontInfo FontInfo = TextBlock->GetFont();
	FontInfo.Size = FontSize;
	TextBlock->SetFont(FontInfo);
}
