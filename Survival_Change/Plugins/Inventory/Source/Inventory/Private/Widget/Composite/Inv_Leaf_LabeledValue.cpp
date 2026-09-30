// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/Composite/Inv_Leaf_LabeledValue.h"

#include "Components/TextBlock.h"

void UInv_Leaf_LabeledValue::SetText_Label(const FText& Text, bool bCollapse) const
{
	if (bCollapse)
	{
		Text_Label->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text_Label->SetText(Text);
}

void UInv_Leaf_LabeledValue::SetText_Value(const FText& Text, bool bCollapse) const
{
	if (bCollapse)
	{
		Text_Value->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	Text_Value->SetText(Text);
}

void UInv_Leaf_LabeledValue::NativePreConstruct()
{
	Super::NativePreConstruct();
	FSlateFontInfo FontInfo = Text_Label->GetFont();
	FontInfo.Size = FontSize_Label;
	Text_Label->SetFont(FontInfo);
	
	FontInfo = Text_Value->GetFont();
	FontInfo.Size = FontSize_Value;
	Text_Value->SetFont(FontInfo);
}
