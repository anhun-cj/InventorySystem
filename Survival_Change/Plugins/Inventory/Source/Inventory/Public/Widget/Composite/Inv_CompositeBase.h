// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Blueprint/UserWidget.h"
#include "Inv_CompositeBase.generated.h"

/**
 * 
 */
UCLASS()
class INVENTORY_API UInv_CompositeBase : public UUserWidget
{
	GENERATED_BODY()
public:
	FGameplayTag GetFragmentTag() const { return FragmentTag; }
	void SetFragmentTag(FGameplayTag Tag) { FragmentTag = Tag; }
	virtual void Collapse();
	void Expand();
	
	using FuncType = TFunction<void(UInv_CompositeBase*)>;
	virtual void ApplyFunction(FuncType Function){}
	
private:
	UPROPERTY(EditAnywhere, Category = "Inventory", meta = (CateGories = "FragmentTags"))
	FGameplayTag FragmentTag;
};
