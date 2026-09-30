// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Misc/MapErrors.h"
#include "Inv_ProxyActor.generated.h"

class UInv_EquipmentComponent;

UCLASS()
class INVENTORY_API AInv_ProxyActor : public AActor
{
	GENERATED_BODY()

public:
	AInv_ProxyActor();
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return Mesh; }
protected:
	virtual void BeginPlay() override;
	
private:
	//玩家的骨骼
	TWeakObjectPtr<USkeletalMeshComponent> SourceMesh;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UInv_EquipmentComponent> EquipmentComponent;
	
	//代理骨骼
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USkeletalMeshComponent> Mesh;
	
	FTimerHandle TimerForNextTick;
	void DelayInitializeOwner();
	void DelayInitialization();
};
