// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Inv_ItemActor.generated.h"

class UInv_HighlightableStaticMesh;
class UInv_ItemComponent;

UCLASS()
class INVENTORY_API AInv_ItemActor : public AActor
{
	GENERATED_BODY()
public:
	AInv_ItemActor();

protected:
	virtual void BeginPlay() override;
	
private:
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<UInv_ItemComponent> ItemComponent;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInv_HighlightableStaticMesh> StaticMeshComponent;
};
