#include "Widget/CharacterDisplay/Inv_CharacterDisplay.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "EquipmentManager/ProxyActor/Inv_ProxyActor.h"
#include "Kismet/GameplayStatics.h"

FReply UInv_CharacterDisplay::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	CurrentPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	LastPosition = CurrentPosition;
	bIsDragging = true;
	return FReply::Handled();
}

FReply UInv_CharacterDisplay::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	bIsDragging = false;
	return FReply::Handled();
}

void UInv_CharacterDisplay::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	TArray<AActor*> Actors;
	UGameplayStatics::GetAllActorsOfClass(this, AInv_ProxyActor::StaticClass(), Actors);
	if (Actors.Num() == 0)
		return;
	AInv_ProxyActor* ProxyActor = Cast<AInv_ProxyActor>(Actors[0]);
	if (!IsValid(ProxyActor))
		return;
	Mesh = ProxyActor->GetSkeletalMeshComponent();
}

void UInv_CharacterDisplay::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	bIsDragging = false;
}

void UInv_CharacterDisplay::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (!bIsDragging)
		return;
	LastPosition = CurrentPosition;
	CurrentPosition = UWidgetLayoutLibrary::GetMousePositionOnViewport(GetOwningPlayer());
	
	const float HorizontalOffset = CurrentPosition.X - LastPosition.X;
	if (!Mesh.IsValid())
		return;
	Mesh->AddRelativeRotation(FRotator(0.0f, HorizontalOffset, 0.0f));
}
