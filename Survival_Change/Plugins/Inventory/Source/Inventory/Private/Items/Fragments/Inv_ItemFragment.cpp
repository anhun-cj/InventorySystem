#include "Items/Fragments/Inv_ItemFragment.h"

#include "EquipmentManager/EquipActor/Inv_EquipActor.h"
#include "Widget/Composite/Inv_CompositeBase.h"
#include "Widget/Composite/Inv_Leaf_Image.h"
#include "Widget/Composite/Inv_Leaf_LabeledValue.h"
#include "Widget/Composite/Inv_Leaf_Text.h"

void FInv_InventoryItemFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	if (!MatchesWidgetTag(Composite))
		return;
	Composite->Expand();
}

bool FInv_InventoryItemFragment::MatchesWidgetTag(const UInv_CompositeBase* Composite) const
{
	return Composite->GetFragmentTag().MatchesTagExact(GetFragmentTag());
}

void FInv_ImageFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	FInv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite))
		return;
	UInv_Leaf_Image* Image = Cast<UInv_Leaf_Image>(Composite);
	if (!IsValid(Image))
		return;
	Image->SetImage(Icon);
	Image->SetBoxSize(IconDimension);
	Image->SetImageSize(IconDimension);
}

void FInv_TextFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	FInv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite))
		return;
	UInv_Leaf_Text* Text = Cast<UInv_Leaf_Text>(Composite);
	if (!IsValid(Text))
		return;
	Text->SetText(FragmentText);
}

void FInv_LabeledNumberFragment::Manifest()
{
	FInv_InventoryItemFragment::Manifest();
	if (bRandomizeOnManifest)
	{
		Value = FMath::RandRange(MinValue, MaxValue);
	}
	bRandomizeOnManifest = false;
}

void FInv_LabeledNumberFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	FInv_InventoryItemFragment::Assimilate(Composite);
	if (!MatchesWidgetTag(Composite))
		return;
	UInv_Leaf_LabeledValue* LabeledValue = Cast<UInv_Leaf_LabeledValue>(Composite);
	if (!IsValid(LabeledValue))
		return;
	LabeledValue->SetText_Label(Text_Label, bCollapseLabel);
	
	FNumberFormattingOptions Options;
	Options.MinimumFractionalDigits = MinFractionalDigits;
	Options.MaximumFractionalDigits = MaxFractionalDigits;
	
	LabeledValue->SetText_Value(FText::AsNumber(Value, &Options), bCollapseLabel);
}

void FInv_ConsumeFragment::OnConsume(APlayerController* PC)
{
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnConsume(PC);
	}
}

void FInv_ConsumeFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	FInv_InventoryItemFragment::Assimilate(Composite);
	for (const auto& Modifier : ConsumeModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

void FInv_ConsumeFragment::Manifest()
{
	FInv_InventoryItemFragment::Manifest();
	for (auto& Modifier : ConsumeModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
}

void FInv_ManaPotionFragment::OnConsume(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("消耗物品:回复生命值%d"), FMath::FloorToInt(GetValue())));
}

void FInv_HealthPotionFragment::OnConsume(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("消耗物品:回复能量%d"), FMath::FloorToInt(GetValue())));
}

void FInv_StrengthModifier::OnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("力量增加%d"), FMath::FloorToInt(GetValue())));
}

void FInv_StrengthModifier::OnUnEquip(APlayerController* PC)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, FString::Printf(TEXT("力量降低%d"), FMath::FloorToInt(GetValue())));

}

void FInv_EquipmentFragment::OnEquip(APlayerController* PC)
{
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnEquip(PC);
	}
	bEquipable = true;
}

void FInv_EquipmentFragment::OnUnEquip(APlayerController* PC)
{
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.OnUnEquip(PC);
	}
	bEquipable = false;
}

void FInv_EquipmentFragment::Manifest()
{
	FInv_InventoryItemFragment::Manifest();
	for (auto& Modifier : EquipModifiers)
	{
		auto& ModRef = Modifier.GetMutable();
		ModRef.Manifest();
	}
	bEquipable = false;
}

void FInv_EquipmentFragment::Assimilate(UInv_CompositeBase* Composite) const
{
	FInv_InventoryItemFragment::Assimilate(Composite);
	for (const auto& Modifier : EquipModifiers)
	{
		const auto& ModRef = Modifier.Get();
		ModRef.Assimilate(Composite);
	}
}

AInv_EquipActor* FInv_EquipmentFragment::SpawnAttachActor(USkeletalMeshComponent* SkeletonMesh) const
{
	if (!IsValid(SkeletonMesh) || !IsValid(EquipActorClass))
		return nullptr;
	AInv_EquipActor* EquippedActor = SkeletonMesh->GetWorld()->SpawnActor<AInv_EquipActor>(EquipActorClass);
	EquippedActor->AttachToComponent(SkeletonMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale, SocketAttachPoint);
	return EquippedActor;
}

void FInv_EquipmentFragment::SetEquipActor(AInv_EquipActor* Actor)
{
	EquipActor = Actor;
}

AInv_EquipActor* FInv_EquipmentFragment::GetEquipActor() const
{
	return EquipActor.Get();
}

void FInv_EquipmentFragment::DestroyAttachActor() const
{
	if (!EquipActor.IsValid())
		return;
	EquipActor->Destroy();
}
