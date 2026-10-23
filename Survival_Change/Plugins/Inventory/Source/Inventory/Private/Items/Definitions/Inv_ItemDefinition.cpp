#include "Items/Definitions/Inv_ItemDefinition.h"

#include "Items/Inv_InventoryItem.h"
#include "Items/Fragments/Inv_FragmentTags.h"
#include "Items/Fragments/Inv_ItemFragment.h"
#include "Items/Manifest/Inv_ItemManifest.h"

const FPrimaryAssetType UInv_ItemDefinition::PrimaryAssetType = TEXT("InvItem");

FPrimaryAssetId UInv_ItemDefinition::GetPrimaryAssetId() const
{
	if (!ItemId.IsNone())
	{
		return FPrimaryAssetId(PrimaryAssetType, ItemId);
	}
	return FPrimaryAssetId(PrimaryAssetType, GetFName());
}

EInv_ItemCategory UInv_ItemDefinition::GetItemCategory() const
{
	if (IsA<UInv_EquipmentItemDefinition>())
		return EInv_ItemCategory::Equippable;
	if (IsA<UInv_ConsumableItemDefinition>())
		return EInv_ItemCategory::Consumable;
	return EInv_ItemCategory::None;
}

bool UInv_ItemDefinition::IsStackable() const
{
	return !IsA<UInv_EquipmentItemDefinition>() && MaxStackSize > 1;
}

bool UInv_ItemDefinition::IsCraftable() const
{
	if (const UInv_EquipmentItemDefinition* Equipment = Cast<UInv_EquipmentItemDefinition>(this))
		return !Equipment->CraftingIngredients.IsEmpty();
	if (const UInv_ConsumableItemDefinition* Consumable = Cast<UInv_ConsumableItemDefinition>(this))
		return !Consumable->CraftingIngredients.IsEmpty();
	return false;
}

bool UInv_ItemDefinition::HasItemType(const FGameplayTag ItemTypeTag) const
{
	return ItemTypeTag.IsValid() && ItemTags.HasTag(ItemTypeTag);
}

namespace InvItemDefinitionInternal
{
	/** 取 ItemTags 中层级最深的标签作为物品类型标签。 */
	static FGameplayTag GetMostSpecificTag(const FGameplayTagContainer& Tags)
	{
		FGameplayTag BestTag;
		int32 BestDepth = -1;
		for (auto It = Tags.CreateConstIterator(); It; ++It)
		{
			const FGameplayTag& Tag = *It;
			const int32 Depth = Tag.GetGameplayTagParents().Num();
			if (Depth > BestDepth)
			{
				BestDepth = Depth;
				BestTag = Tag;
			}
		}
		return BestTag;
	}

	static FText GetStatLabel(const FGameplayTag& StatTag)
	{
		if (!StatTag.IsValid())
		{
			return FText::GetEmpty();
		}
		const FName Name = StatTag.GetTagLeafName();
		if (Name == TEXT("Strength"))
			return NSLOCTEXT("Inventory", "StrengthStat", "力量");
		if (Name == TEXT("Health"))
			return NSLOCTEXT("Inventory", "HealthStat", "生命值");
		if (Name == TEXT("Mana"))
			return NSLOCTEXT("Inventory", "ManaStat", "法力值");
		return FText::FromString(StatTag.GetTagLeafName().ToString());
	}

	static void FillLabeledNumber(FInv_LabeledNumberFragment& Fragment, const FGameplayTag& StatTag, float Value)
	{
		Fragment.SetLabel(GetStatLabel(StatTag));
		Fragment.SetValue(Value);
	}

	/**
	 * 按属性标签映射到具体的装备修改器片段。
	 * 已知标签（如 ...Strength）映射到带行为的具体片段；未知标签退化为仅携带数值的基础修改器。
	 */
	static TInstancedStruct<FInv_EquipModifier> MakeEquipModifier(const FInv_ItemStatModifier& Stat)
	{
		TInstancedStruct<FInv_EquipModifier> Result;
		const FName LeafName = Stat.StatTag.GetTagLeafName();
		if (LeafName == TEXT("Strength"))
		{
			FInv_StrengthModifier Modifier;
			FillLabeledNumber(Modifier, Stat.StatTag, Stat.Value);
			Result = TInstancedStruct<FInv_EquipModifier>::Make<FInv_StrengthModifier>(Modifier);
		}
		else
		{
			FInv_EquipModifier Modifier;
			FillLabeledNumber(Modifier, Stat.StatTag, Stat.Value);
			Result = TInstancedStruct<FInv_EquipModifier>::Make<FInv_EquipModifier>(Modifier);
		}
		return Result;
	}

	/** 按效果标签映射到具体的消耗修改器片段；未知标签退化为仅携带数值的基础修改器。 */
	static TInstancedStruct<FInv_ConsumeModifier> MakeConsumeModifier(const FInv_ItemStatModifier& Effect)
	{
		TInstancedStruct<FInv_ConsumeModifier> Result;
		const FName LeafName = Effect.StatTag.GetTagLeafName();
		if (LeafName == TEXT("Health"))
		{
			FInv_HealthPotionFragment Modifier;
			FillLabeledNumber(Modifier, Effect.StatTag, Effect.Value);
			Result = TInstancedStruct<FInv_ConsumeModifier>::Make<FInv_HealthPotionFragment>(Modifier);
		}
		else if (LeafName == TEXT("Mana"))
		{
			FInv_ManaPotionFragment Modifier;
			FillLabeledNumber(Modifier, Effect.StatTag, Effect.Value);
			Result = TInstancedStruct<FInv_ConsumeModifier>::Make<FInv_ManaPotionFragment>(Modifier);
		}
		else
		{
			FInv_ConsumeModifier Modifier;
			FillLabeledNumber(Modifier, Effect.StatTag, Effect.Value);
			Result = TInstancedStruct<FInv_ConsumeModifier>::Make<FInv_ConsumeModifier>(Modifier);
		}
		return Result;
	}
}

void UInv_ItemDefinition::BuildItemManifest(FInv_ItemManifest& Manifest) const
{
	Manifest.SetItemId(ItemId);
	Manifest.SetSourceDefinition(const_cast<UInv_ItemDefinition*>(this));
	Manifest.SetItemCategory(GetItemCategory());
	Manifest.SetItemType(InvItemDefinitionInternal::GetMostSpecificTag(ItemTags));
	TArray<TInstancedStruct<FInv_ItemFragment>>& Fragments = Manifest.GetAllFragments();
	Fragments.Reset();

	FInv_GridFragment GridFragment;
	GridFragment.SetFragmentTag(FragmentTags::GridFragment);
	GridFragment.SetGridSize(GridSize);
	GridFragment.SetGridPadding(GridPadding);
	Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_GridFragment>(GridFragment));

	FInv_ImageFragment ImageFragment;
	ImageFragment.SetFragmentTag(FragmentTags::IconFragment);
	ImageFragment.SetIcon(Icon);
	Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_ImageFragment>(ImageFragment));

	FInv_TextFragment NameFragment;
	NameFragment.SetFragmentTag(FragmentTags::ItemNameFragment);
	NameFragment.SetText(DisplayName);
	Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(NameFragment));
	FInv_TextFragment TypeFragment;
	TypeFragment.SetFragmentTag(FragmentTags::ItemTypeFragment);
	TypeFragment.SetText(GetItemCategory() == EInv_ItemCategory::Equippable ? NSLOCTEXT("Inventory", "EquipmentType", "装备")
		: GetItemCategory() == EInv_ItemCategory::Consumable ? NSLOCTEXT("Inventory", "ConsumableType", "消耗品")
		: NSLOCTEXT("Inventory", "CommonType", "普通物品"));
	Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(TypeFragment));

	if (!Description.IsEmpty())
	{
		FInv_TextFragment FlavorFragment;
		FlavorFragment.SetFragmentTag(FragmentTags::FlavorTextFragment);
		FlavorFragment.SetText(Description);
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(FlavorFragment));
	}

	if (IsStackable())
	{
		FInv_StackableFragment StackableFragment;
		StackableFragment.SetFragmentTag(FragmentTags::StackableFragment);
		StackableFragment.SetMaxStackCount(MaxStackSize);
		StackableFragment.SetStackCount(FMath::Clamp(DefaultStackCount, 1, MaxStackSize));
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_StackableFragment>(StackableFragment));
	}

	const TArray<FInv_ItemStatModifier>* Effects = nullptr;
	const TArray<FInv_ItemAmount>* Ingredients = nullptr;
	FGameplayTag EffectsTag;
	FText EffectsTitle;
	if (const UInv_EquipmentItemDefinition* Equipment = Cast<UInv_EquipmentItemDefinition>(this))
	{
		Effects = &Equipment->BaseStats;
		Ingredients = &Equipment->CraftingIngredients;
		EffectsTag = FragmentTags::EquipmentEffectsFragment;
		EffectsTitle = NSLOCTEXT("Inventory", "EquipmentEffectsTitle", "装备效果");
		FInv_EquipmentFragment EquipmentFragment;
		EquipmentFragment.SetFragmentTag(FragmentTags::EquipmentFragment);
		EquipmentFragment.SetEquipTag(Equipment->EquipmentSlotTag);
		EquipmentFragment.SetEquipActorClass(Equipment->EquipActorClass);
		EquipmentFragment.SetSocketAttachPoint(Equipment->SocketAttachPoint);
		for (const FInv_ItemStatModifier& Stat : Equipment->BaseStats)
		{
			EquipmentFragment.GetEquipModifiersMutable().Add(InvItemDefinitionInternal::MakeEquipModifier(Stat));
		}
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_EquipmentFragment>(EquipmentFragment));
	}
	else if (const UInv_ConsumableItemDefinition* Consumable = Cast<UInv_ConsumableItemDefinition>(this))
	{
		Effects = &Consumable->ConsumeEffects;
		Ingredients = &Consumable->CraftingIngredients;
		EffectsTag = FragmentTags::ConsumeEffectsFragment;
		EffectsTitle = NSLOCTEXT("Inventory", "ConsumeEffectsTitle", "消耗效果");
		FInv_ConsumeFragment ConsumeFragment;
		ConsumeFragment.SetFragmentTag(FragmentTags::ConsumableFragment);
		for (const FInv_ItemStatModifier& Effect : Consumable->ConsumeEffects)
		{
			ConsumeFragment.GetConsumeModifiersMutable().Add(InvItemDefinitionInternal::MakeConsumeModifier(Effect));
		}
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_ConsumeFragment>(ConsumeFragment));
	}
	else if (!AcquisitionSources.IsEmpty())
	{
		FInv_TextFragment AcquisitionFragment;
		AcquisitionFragment.SetFragmentTag(FragmentTags::AcquisitionSourcesFragment);
		AcquisitionFragment.SetText(FText::Format(NSLOCTEXT("Inventory", "AcquisitionText", "获取途径\n{0}"), AcquisitionSources));
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(AcquisitionFragment));
	}

	// 详情与实际修改器读取相同的效果数组，不另存显示数值，也不限制效果条数。
	if (Effects && !Effects->IsEmpty())
	{
		TArray<FString> Lines;
		for (const FInv_ItemStatModifier& Effect : *Effects)
			Lines.Add(FText::Format(NSLOCTEXT("Inventory", "ItemEffectLine", "{0} {1}{2}"),
				InvItemDefinitionInternal::GetStatLabel(Effect.StatTag),
				FText::FromString(Effect.Value >= 0 ? TEXT("+") : TEXT("")), FText::AsNumber(Effect.Value)).ToString());
		FInv_TextFragment EffectsFragment;
		EffectsFragment.SetFragmentTag(EffectsTag);
		EffectsFragment.SetText(FText::Format(NSLOCTEXT("Inventory", "ItemEffectsText", "{0}\n{1}"),
			EffectsTitle, FText::FromString(FString::Join(Lines, TEXT("\n")))));
		Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(EffectsFragment));
	}
	if (Ingredients)
	{
		TArray<FString> Lines;
		for (const FInv_ItemAmount& Ingredient : *Ingredients)
		{
			const UInv_ItemDefinition* Material = Ingredient.ItemDefinition.LoadSynchronous();
			if (Material)
				Lines.Add(FText::Format(NSLOCTEXT("Inventory", "CraftingMaterialLine", "{0} ×{1}"),
					Material->DisplayName, FText::AsNumber(Ingredient.Count)).ToString());
		}
		if (!Lines.IsEmpty())
		{
			FInv_TextFragment MaterialsFragment;
			MaterialsFragment.SetFragmentTag(FragmentTags::CraftingMaterialsFragment);
			MaterialsFragment.SetText(FText::Format(NSLOCTEXT("Inventory", "CraftingMaterialsText", "制作所需材料\n{0}"),
				FText::FromString(FString::Join(Lines, TEXT("\n")))));
			Fragments.Add(TInstancedStruct<FInv_ItemFragment>::Make<FInv_TextFragment>(MaterialsFragment));
		}
	}
}

UInv_InventoryItem* UInv_ItemDefinition::ManifestItem(UObject* Outer) const
{
	if (!IsValid(Outer))
	{
		return nullptr;
	}

	FInv_ItemManifest Manifest;
	BuildItemManifest(Manifest);
	return Manifest.Manifest(Outer);
}
