#include "EquipmentManager/ProxyActor/Inv_ProxyActor.h"

#include "EquipmentManager/Components/Inv_EquipmentComponent.h"
#include "GameFramework/Character.h"


AInv_ProxyActor::AInv_ProxyActor()
{
	PrimaryActorTick.bCanEverTick = false;
	
	SetReplicates(false);
	RootComponent = CreateDefaultSubobject<USceneComponent>("RootComponent");
	Mesh = CreateDefaultSubobject<USkeletalMeshComponent>("Mesh");
	Mesh->SetupAttachment(RootComponent);
	EquipmentComponent = CreateDefaultSubobject<UInv_EquipmentComponent>("EquipmentComponent");
	EquipmentComponent->SetOwningSkeletalMeshComponent(Mesh);
	EquipmentComponent->SetProxy(true);
}

void AInv_ProxyActor::BeginPlay()
{
	Super::BeginPlay();
	DelayInitializeOwner();
}

void AInv_ProxyActor::DelayInitializeOwner()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		DelayInitialization();
		return;
	}
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!IsValid(PC))
	{
		DelayInitialization();
		return;
	}
	ACharacter* Character = Cast<ACharacter>(PC->GetPawn());
	if (!IsValid(Character))
	{
		DelayInitialization();
		return;
	}
	USkeletalMeshComponent* SkeletalMesh = Character->GetMesh();
	if (!IsValid(SkeletalMesh))
	{
		DelayInitialization();
		return;
	}
	
	SourceMesh = SkeletalMesh;
	Mesh->SetSkeletalMesh(SkeletalMesh->GetSkeletalMeshAsset());
	Mesh->SetAnimInstanceClass(SourceMesh->GetAnimInstance()->GetClass());
	EquipmentComponent->InitializeOwner(PC);
}

void AInv_ProxyActor::DelayInitialization()
{
	FTimerDelegate TimerDelegate;
	TimerDelegate.BindUObject(this, &AInv_ProxyActor::DelayInitializeOwner);
	GetWorld()->GetTimerManager().SetTimerForNextTick(TimerDelegate);
}

