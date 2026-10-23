#pragma once

#include "CoreMinimal.h"
#include "InventoryManagement/Components/Inv_InventoryComponent.h"
#include "Inv_FindItemComponent.generated.h"

class UInv_OpenInventoryWidget;
class UInv_InventoryGrid;
class UInv_InventoryItem;
class UInv_ItemComponent;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_FindItemComponent : public UInv_InventoryComponent
{
	GENERATED_BODY()

public:
	UInv_FindItemComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void SetInventoryGrid(UInv_InventoryGrid* Grid);
	void SplitNearbyItem(UInv_InventoryItem* Item, int32 Count);

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "FindItem")
	float DetectRadius = 200.0f;
	
	UFUNCTION()
	void OnItemAdd(UInv_InventoryItem* Item);

	UFUNCTION()
	void OnItemChange(UInv_InventoryItem* Item);

	UFUNCTION()
	void OnItemRemove(UInv_InventoryItem* Item);

private:
	friend class UInv_RequestComponent;
	friend class UInv_InventoryComponent;
	UFUNCTION(Server, Reliable)
	void Server_RequestPickUp(UInv_ItemComponent* ItemComponent);

	UFUNCTION(Server, Reliable)
	void Server_ClearItemComponents();

	UInv_ItemComponent* FindItemComponent(UInv_InventoryItem* Item);
	
	void ClearNoValidItemComponents();
	
	void AddItemOnServer(UInv_ItemComponent* ItemComponent);

	UPROPERTY(Transient)
	TMap<TWeakObjectPtr<UInv_InventoryItem>, TWeakObjectPtr<UInv_ItemComponent>> ItemComponents;

	//已检测过的物品，防止重复检查
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UInv_ItemComponent>> DetectedComponents;

	TWeakObjectPtr<UInv_InventoryGrid> InventoryGrid;
};
