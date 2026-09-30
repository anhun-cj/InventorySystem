#include "Player/Inv_PlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "Widget/HUD/Inv_HUDWidget.h"
#include "Interaction/Inv_Highlightable.h"
#include "Items/Components/Inv_ItemComponent.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Kismet/GameplayStatics.h"

AInv_PlayerController::AInv_PlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
	TraceLength = 500.f;
	ItemTraceChannel = ECC_GameTraceChannel1;
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
	InventoryComponent = FindComponentByClass<UInv_InventoryComponent>();
	CreateHUDWidget();
}

void AInv_PlayerController::PrimaryInteract()
{
	//UE_LOG(LogTemp, Log, TEXT("Primary Test"));
	
	if (!ThisActor.IsValid())
		return;
	UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
	if (!IsValid(ItemComponent) || !InventoryComponent.IsValid())
		return;
	InventoryComponent->TryAddItem(ItemComponent);
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
	
	LastActor = ThisActor;
	ThisActor = HitResult.GetActor();

	if (!ThisActor.IsValid())
	{
		if (IsValid(HUDWidget))
			HUDWidget->HidePickUpMessage();
	}

	if (LastActor == ThisActor)
		return;
	if (ThisActor.IsValid())
	{
		//UE_LOG(LogTemp, Warning, TEXT("Start Tracing A New Actor"));

		UActorComponent* Highlightable = ThisActor->FindComponentByInterface(UInv_Highlightable::StaticClass());
		if (IsValid(Highlightable))
			IInv_Highlightable::Execute_Highlight(Highlightable);

		UInv_ItemComponent* ItemComponent = ThisActor->FindComponentByClass<UInv_ItemComponent>();
		if (!IsValid(ItemComponent))
			return;
		if (IsValid(HUDWidget))
			HUDWidget->ShowPickUpMessage(ItemComponent->GetPickUpMessage());
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
	TraceForItem();
}

void AInv_PlayerController::ToggleInventory()
{
	if (!InventoryComponent.IsValid())
		return;
	InventoryComponent->ToggleInventoryMenu();
	if (InventoryComponent->IsInventoryMenuOpen())
	{
		HUDWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	else
	{
		HUDWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}
