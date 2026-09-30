#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryManagement/FastArrary/Inv_FastArray.h"
#include "Inv_InventoryComponent.generated.h"

struct FInv_SpaceQueryResult;
struct FInv_SlotAvailabilityResult;
class UInv_InventoryBase;
class UInv_InventoryItem;
class UInv_ItemComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryChange, UInv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemEquipStatusChanged, UInv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FInventoryMenuToggled, bool, bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryRemoveSlottedItem, UInv_InventoryItem*, Item, int32, GridIndex);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInv_InventoryComponent();

	UFUNCTION(BlueprintCallable)
	void MoveItemOnServer(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex);
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void TryAddItem(UInv_ItemComponent* ItemComponent);
	void ChangeItemIndex(UInv_InventoryItem* Item, int32 Index);

	UFUNCTION(Server, Reliable)
	void Server_AddItem(UInv_ItemComponent* ItemComponent, const int32 StackCount, const int32 GridIndex, UInv_InventoryItem* Item, const bool IsInGridSlots);
	
	UFUNCTION(Server, Reliable)
	void Server_MoveItem(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex);
	
	UFUNCTION(Server, Reliable)
	void Server_RemoveSlottedItem(UInv_InventoryItem* SourceItem);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RemoveSlottedItem(UInv_InventoryItem* SourceItem);
	
	UFUNCTION(Server, Reliable)
	void Server_DestroyActor(UInv_ItemComponent* ItemComponent, int32 Remainder);
	
	UFUNCTION(Server, Reliable)
	void Server_AddNewItem(UInv_ItemComponent* ItemComponent, int32 StackCount);
	
	UFUNCTION(Server, Reliable)
	void Server_AddStacksToItem(UInv_ItemComponent* ItemComponent, int32 StackCount, int32 Remainder);
	
	UFUNCTION(Server, Reliable)
	void Server_DropItem(UInv_InventoryItem* Item);
	
	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UInv_InventoryItem* Item, int32 GridIndex);
	
	UFUNCTION(Server, Reliable)
	void Server_EquipSlotClicked(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipSlotClicked(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);
	
	void ToggleInventoryMenu();

	void AddRepSubObj(UObject* SubObj);
	
	void SpawnDroppedItem(UInv_InventoryItem* Item) const;
	
	UFUNCTION(BlueprintCallable)
	UInv_InventoryBase* GetInventoryMenu() const { return InventoryMenu; }
	
	bool IsInventoryMenuOpen() const { return bInventoryMenuOpen; }
	
	UInv_InventoryItem* FindNoFillItemByType(const FGameplayTag& Tag, const TSet<UInv_InventoryItem*> Items) const;
	
	FInventoryChange OnItemAdded;
	FInventoryChange OnItemRemoved;
	FInventoryChange OnItemChanged;
	FInventoryRemoveSlottedItem OnRemoveSlottedItem;
	FNoRoomInInventory NoRoomInInventory;
	FItemEquipStatusChanged OnItemEquipped;
	FItemEquipStatusChanged OnItemUnequipped;
	FInventoryMenuToggled OnInventoryMenuToggled;
protected:
	virtual void BeginPlay() override;

private:
	void ConstructInventory();

	TWeakObjectPtr<APlayerController> OwningController;

	UPROPERTY(Replicated)
	FInv_InventoryFastArray InventoryList;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_InventoryBase> InventoryMenuClass;

	UPROPERTY();
	TObjectPtr<UInv_InventoryBase> InventoryMenu;

	bool bInventoryMenuOpen;

	void OpenInventoryMenu();
	void CloseInventoryMenu();
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMin = -85.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnAngleMax = 85.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMin = 50.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMax = 120.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float RelativeSpawnElevation = -70.0f;
};
