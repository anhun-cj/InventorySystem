#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "Inv_ItemFragment.generated.h"

class AInv_EquipActor;
class UInv_CompositeBase;
class APlayerController;

USTRUCT(BlueprintType)
struct FInv_ItemFragment
{
	GENERATED_BODY()
	
	FInv_ItemFragment() {}
	FInv_ItemFragment(const FInv_ItemFragment&) = default;
	FInv_ItemFragment& operator=(const FInv_ItemFragment&) = default;
	FInv_ItemFragment(FInv_ItemFragment&&) = default;
	FInv_ItemFragment& operator=(FInv_ItemFragment&&) = default;
	virtual ~FInv_ItemFragment() {}
	
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(FGameplayTag Tag) { FragmentTag = Tag; }
	
	virtual void Manifest() {}
private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (Categories = "FragmentTags"))
	FGameplayTag FragmentTag = FGameplayTag::EmptyTag;
};

/*
 * 用于集成到一个UI页面
 */

USTRUCT(BlueprintType)
struct FInv_InventoryItemFragment : public FInv_ItemFragment
{
	GENERATED_BODY()
	virtual void Assimilate(UInv_CompositeBase* Composite) const;
protected:
	bool MatchesWidgetTag(const UInv_CompositeBase* Composite) const;
};

USTRUCT(BlueprintType)
struct FInv_GridFragment : public FInv_ItemFragment
{
	GENERATED_BODY()
	
	FIntPoint GetGridSize() const { return GridSize; }
	void SetGridSize(const FIntPoint& Size) { GridSize = Size; }
	
	float GetGridPadding() const { return GridPadding; }
	void SetGridPadding(float Padding) { GridPadding = Padding; }
	
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FIntPoint GridSize{1,1};
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float GridPadding{0.0f};
};

USTRUCT(BlueprintType)
struct FInv_ImageFragment : public FInv_InventoryItemFragment
{
	GENERATED_BODY()
	
	UTexture2D* GetIcon() const { return Icon; }
	FVector2D GetIconDimension() const { return IconDimension; }
	virtual void Assimilate(UInv_CompositeBase* Composite) const override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TObjectPtr<UTexture2D> Icon{ nullptr };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FVector2D IconDimension{ 44.0f, 44.0f };
};

USTRUCT(BlueprintType)
struct FInv_TextFragment : public FInv_InventoryItemFragment
{
	GENERATED_BODY()
	FText GetText() const { return FragmentText; }
	void SetText(const FText& Text) { FragmentText = Text; }
	virtual void Assimilate(UInv_CompositeBase* Composite) const override;
	
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FText FragmentText;
};

USTRUCT(BlueprintType)
struct FInv_LabeledNumberFragment : public FInv_InventoryItemFragment
{
	GENERATED_BODY()
	
	virtual void Manifest() override;
	virtual void Assimilate(UInv_CompositeBase* Composite) const override;
	float GetValue() const { return Value; }
	
	bool bRandomizeOnManifest{ true };
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FText Text_Label;
	
	UPROPERTY(VisibleAnywhere, Category = "Inventory")
	float Value = {0};
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float MinValue{ 0.0f };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float MaxValue{ 0.0f };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	bool bCollapseLabel{ false };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	bool bCollapseValue{ false };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MinFractionalDigits{ 1 };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MaxFractionalDigits{ 1 };
};

USTRUCT(BlueprintType)
struct FInv_StackableFragment : public FInv_ItemFragment
{
	GENERATED_BODY()
	
	int32 GetMaxStackCount() const { return MaxStackCount; }
	int32 GetStackCount() const { return StackCount; }
	void SetStackCount(int32 Count) { StackCount = Count; }
	
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 MaxStackCount{ 1 };
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	int32 StackCount{ 1 };
};

USTRUCT(BlueprintType)
struct FInv_ConsumeModifier : public FInv_LabeledNumberFragment
{
	GENERATED_BODY()
	virtual void OnConsume(APlayerController* PC) {}
};

USTRUCT(BlueprintType)
struct FInv_ConsumeFragment : public FInv_InventoryItemFragment
{
	GENERATED_BODY()
	
	virtual void OnConsume(APlayerController* PC);
	virtual void Assimilate(UInv_CompositeBase* Composite) const override;
	virtual void Manifest() override; 
private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (ExcludeBaseStruct))
	TArray<TInstancedStruct<FInv_ConsumeModifier>> ConsumeModifiers;
};

USTRUCT(BlueprintType)
struct FInv_ManaPotionFragment : public FInv_ConsumeModifier
{
	GENERATED_BODY()
	
	virtual void OnConsume(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FInv_HealthPotionFragment : public FInv_ConsumeModifier
{
	GENERATED_BODY()
	
	virtual void OnConsume(APlayerController* PC) override;
};

//装备片段

USTRUCT(BlueprintType)
struct FInv_EquipModifier : public FInv_LabeledNumberFragment
{
	GENERATED_BODY()
	virtual void OnEquip(APlayerController* PC){}
	virtual void OnUnEquip(APlayerController* PC){}
};

USTRUCT(BlueprintType)
struct FInv_StrengthModifier : public FInv_EquipModifier
{
	GENERATED_BODY()
	virtual void OnEquip(APlayerController* PC) override;
	virtual void OnUnEquip(APlayerController* PC) override;
};

USTRUCT(BlueprintType)
struct FInv_EquipmentFragment : public FInv_InventoryItemFragment
{
	GENERATED_BODY()
	
	bool bEquipable{ false };
	
	void OnEquip(APlayerController* PC);
	void OnUnEquip(APlayerController* PC);
	virtual void Manifest() override; 
	virtual void Assimilate(UInv_CompositeBase* Composite) const override;
	
	AInv_EquipActor* SpawnAttachActor(USkeletalMeshComponent* SkeletonMesh) const;
	void SetEquipActor(AInv_EquipActor* Actor);
	AInv_EquipActor* GetEquipActor() const;
	void DestroyAttachActor() const;
	
	FGameplayTag GetEquipTag() const{ return EquipTag; }
private:
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TArray<TInstancedStruct<FInv_EquipModifier>> EquipModifiers;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<AInv_EquipActor> EquipActorClass = nullptr;
	
	TWeakObjectPtr<AInv_EquipActor> EquipActor = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FName SocketAttachPoint{NAME_None};
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	FGameplayTag EquipTag = FGameplayTag::EmptyTag;
};
