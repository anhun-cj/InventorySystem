#include "Player/Inv_PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Building/Inv_Building.h"
#include "Building/Inv_WareHouse.h"
#include "Engine/OverlapResult.h"
#include "Widget/HUD/Inv_HUDWidget.h"
#include "Interaction/Inv_Highlightable.h"
#include "InventoryManagement/Components/Inv_FindItemComponent.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "InventoryManagement/Utils/Inv_InventoryStatics.h"
#include "InventoryManagement/Components/Inv_RequestComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Widget/WareHouse/Inv_WareHouseWidget.h"

AInv_PlayerController::AInv_PlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	TraceLength = 500.f;
	ItemTraceChannel = ECC_GameTraceChannel1;
	
	RequestComponent = CreateDefaultSubobject<UInv_RequestComponent>("RequestComponent");
	OtherInventoryComponent = CreateDefaultSubobject<UInv_FindItemComponent>("OtherInventoryComponent");
}

void AInv_PlayerController::BeginPlay()
{
	Super::BeginPlay();

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if(IsValid(Subsystem))
	{
		Subsystem->AddMappingContext(DefaultIMC, 0);
		for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}

		for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
		{
			Subsystem->AddMappingContext(CurrentContext, 0);
		}
	}
	
	InventoryComponent = UInv_InventoryStatics::GetInventoryComponent(this);
	CreateHUDWidget();
}

void AInv_PlayerController::PrimaryInteract()
{
	//UE_LOG(LogTemp, Log, TEXT("Primary Test"));
	
	if (!ThisActor.IsValid())
		return;
	UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
	if (IsValid(ItemComponent) && InventoryComponent.IsValid())
	{
		InventoryComponent->TryAddItem(ItemComponent);
		return;
	}
	
	UInv_InventoryComponent* Inventory = ThisActor->FindComponentByClass<UInv_InventoryComponent>();
	if (IsValid(Inventory))
	{
		Inventory->ToggleInventoryMenu();
		UInv_WareHouseWidget* WareHouseWidget = Cast<UInv_WareHouseWidget>(Inventory->GetInventoryMenu());
		WareHouseWidget->InitWareHouseWidget(InventoryComponent.Get(), Inventory);
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
		OpenInventoryComponent = Inventory;
		return;
	}
	
}

void AInv_PlayerController::CreateHUDWidget()
{
	if (IsLocalPlayerController())
		HUDWidget = CreateWidget<UInv_HUDWidget>(this, HUDWidgetClass);
	if (IsValid(HUDWidget))
	{
		HUDWidget->AddToViewport();
	}
}

void AInv_PlayerController::TraceForItem()
{
	if (!IsValid(GEngine) || !IsValid(GEngine->GameViewport))
		return;
	FVector2D ViewportSize;
	GEngine->GameViewport->GetViewportSize(ViewportSize);
	const FVector2D ViewportCenter = ViewportSize / 2.0f;

	FVector TraceStart;
	FVector Forward;

	if (!UGameplayStatics::DeprojectScreenToWorld(this, ViewportCenter, TraceStart, Forward))
		return;

	const FVector TraceEnd = TraceStart + Forward * TraceLength;

	FHitResult HitResult;
	GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ItemTraceChannel);

	AActor* HitActor = HitResult.GetActor();

	// 线性射线未命中时，在射线尖端做一次球形检测，扩大拾取范围
	if (!HitActor && TipSphereRadius > 0.0)
	{
		FCollisionQueryParams SphereQueryParams;
		SphereQueryParams.AddIgnoredActor(this);
		if (APawn* ControlledPawn = GetPawn())
			SphereQueryParams.AddIgnoredActor(ControlledPawn);

		TArray<FOverlapResult> OverlapResults;
		const FCollisionShape TipSphere = FCollisionShape::MakeSphere(static_cast<float>(TipSphereRadius));
		if (GetWorld()->OverlapMultiByChannel(OverlapResults, TraceEnd, FQuat::Identity,
			ItemTraceChannel, TipSphere, SphereQueryParams))
		{
			// 取距离尖端最近的 Actor
			double ClosestDistSq = TNumericLimits<double>::Max();
			for (const FOverlapResult& Overlap : OverlapResults)
			{
				if (AActor* OverlapActor = Overlap.GetActor())
				{
					const double DistSq = FVector::DistSquared(OverlapActor->GetActorLocation(), TraceEnd);
					if (DistSq < ClosestDistSq)
					{
						ClosestDistSq = DistSq;
						HitActor = OverlapActor;
					}
				}
			}
		}
	}

	LastActor = ThisActor;
	ThisActor = HitActor;

	if (!ThisActor.IsValid())
	{
		if (IsValid(HUDWidget))
			HUDWidget->HidePickUpMessage();
	}

	if (LastActor == ThisActor)
		return;
	if (ThisActor.IsValid())
	{
		UActorComponent* Highlightable = ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
		if (IsValid(Highlightable))
			IInv_Highlightable::Execute_Highlight(Highlightable);

		UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
		if (IsValid(ItemComponent) && IsValid(HUDWidget))
		{
			HUDWidget->ShowPickUpMessage(ItemComponent->GetPickUpMessage());
		}
		else if (const AInv_Building* Building = Cast<AInv_Building>(ThisActor))
		{
			HUDWidget->ShowPickUpMessage(Building->GetOpenMessage());
		}
	}

	if (LastActor.IsValid())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Stop Tracing Last Actor"));
		UActorComponent* Highlightable = LastActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
		if (IsValid(Highlightable))
			IInv_Highlightable::Execute_UnHighlight(Highlightable);
	}
}

void AInv_PlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (IsValid(EnhancedInputComponent))
	{
		EnhancedInputComponent->BindAction(PrimaryInteractAction, ETriggerEvent::Started, this, &AInv_PlayerController::PrimaryInteract);
		EnhancedInputComponent->BindAction(ToggleInventoryAction, ETriggerEvent::Started, this, &AInv_PlayerController::ToggleInventory);
	}
}

void AInv_PlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if ((InventoryComponent.IsValid() && InventoryComponent->IsInventoryMenuOpen()) || (OpenInventoryComponent.IsValid() && OpenInventoryComponent->IsInventoryMenuOpen()))
		return;
	if (IsValid(HUDWidget))
		HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	TraceForItem();
}

bool AInv_PlayerController::IsOpenInventoryMenu() const
{
	if (OpenInventoryComponent.IsValid() && OpenInventoryComponent->IsInventoryMenuOpen())
		return true;
	if (InventoryComponent.IsValid() && InventoryComponent->IsInventoryMenuOpen())
		return true;
	return false;
}

void AInv_PlayerController::ToggleInventory()
{
	if (!InventoryComponent.IsValid() || (OpenInventoryComponent.IsValid() && OpenInventoryComponent->IsInventoryMenuOpen()))
		return;
	InventoryComponent->ToggleInventoryMenu();
	if (InventoryComponent->IsInventoryMenuOpen())
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
