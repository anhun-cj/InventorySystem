#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InventoryManagement/FastArrary/Inv_FastArray.h"
#include "Inv_InventoryComponent.generated.h"

struct FInv_SpaceQueryResult;
struct FInv_SlotAvailabilityResult;
struct FInv_ItemManifest;
class UInv_InventoryBase;
class UInv_InventoryItem;
class UInv_ItemComponent;
class UInv_InventoryGrid;
class UInv_OpenInventoryWidget;
class UInv_InventoryGridAsset;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FNoRoomInInventory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FItemEquipStatusChanged, UInv_InventoryItem*, Item);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryRemoveSlottedItem, UInv_InventoryItem*, Item, int32, GridIndex);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), Blueprintable)
class INVENTORY_API UInv_InventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UInv_InventoryComponent();

	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_OpenInventoryWidget> OpenInventoryClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInv_InventoryGridAsset> GridAsset;

	UFUNCTION(BlueprintCallable)
	void MoveItemOnServer(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex);
	bool MoveItemToThisInventory(UInv_InventoryItem& SourceItem, const FInv_SpaceQueryResult& Preview, int32 DropIndex, UInv_InventoryItem* TargetContainer = nullptr);
	
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void TryAddItem(UInv_ItemComponent* ItemComponent);
	
	bool AddItem(UInv_InventoryItem* Item, int32& GridIndex, UInv_InventoryGrid* Grid = nullptr, int32* OutSubGridIndex = nullptr) const;
	void SetBoundInventoryGrid(UInv_InventoryGrid* Grid);
	void ChangeItemIndex(UInv_InventoryItem* Item, int32 Index);
	
	UFUNCTION(Server, Reliable)
	void Server_AddInventoryItem(const int32 GridIndex, UInv_InventoryItem* Item, int32 StackCount, bool IsInGridSlots);
	
	UFUNCTION(Server, Reliable)
	void Server_MoveItem(UInv_InventoryItem* SourceItem, const FInv_SpaceQueryResult& Result, int32 DropIndex);
	
	UFUNCTION(Server, Reliable)
	void Server_RemoveSlottedItem(UInv_InventoryItem* SourceItem);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_RemoveSlottedItem(UInv_InventoryItem* SourceItem);
	
	UFUNCTION(Server, Reliable)
	void Server_DropItem(UInv_InventoryItem* Item);
	
	void Server_RemoveItem(UInv_InventoryItem* Item);
	
	UFUNCTION(Server, Reliable)
	void Server_ConsumeItem(UInv_InventoryItem* Item, int32 GridIndex);
	
	UFUNCTION(Server, Reliable)
	void Server_EquipItem(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip, int32 ItemDropIndex, bool bAllowEquip = true);
	
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_EquipItem(UInv_InventoryItem* ItemToEquip, UInv_InventoryItem* ItemToUnequip);
	
	void ToggleInventoryMenu();

	void AddRepSubObj(UObject* SubObj);
	void RemoveRepSubObj(UObject* SubObj);

	FInv_InventoryFastArray& GetInventoryListMutable() { return InventoryList; }

	UInv_ItemComponent* SpawnDroppedItem(UInv_InventoryItem* Item, const APawn* DropPawn = nullptr) const;
	bool DropItemForPawn(UInv_InventoryItem* Item, const APawn* DropPawn);
	
	UFUNCTION(BlueprintCallable)
	UInv_InventoryBase* GetInventoryMenu() const { return InventoryMenu; }
	
	bool IsInventoryMenuOpen() const { return bInventoryMenuOpen; }
	
	UInv_InventoryItem* FindNoFillItem(const FInv_ItemManifest& Source, const TSet<UInv_InventoryItem*>& Items) const;
	
	const FInv_InventoryFastArray& GetInventoryList() const { return InventoryList;}
	
	void OtherOpenInventoryMenu(UInv_InventoryComponent* Inventory);
	
	FInventoryChange OnItemAdded;
	FInventoryChange OnItemRemoved;
	FInventoryChange OnItemChanged;
	FInventoryRemoveSlottedItem OnRemoveSlottedItem;
	FNoRoomInInventory NoRoomInInventory;
	FItemEquipStatusChanged OnItemEquipped;
	FItemEquipStatusChanged OnItemUnequipped;
protected:
	virtual void BeginPlay() override;

	FVector GetDropSpawnLocation(const APawn* Pawn) const;
	void UpdatePickupActor(UInv_ItemComponent* ItemComponent, int32 Remainder);
	
	UPROPERTY(Replicated)
	FInv_InventoryFastArray InventoryList;
private:
	UFUNCTION(Server, Reliable)
	void Server_PickupItem(UInv_ItemComponent* ItemComponent);

	UFUNCTION(Client, Reliable)
	void Client_NoRoomInInventory();
	
	void ConstructInventory();
	
	TWeakObjectPtr<APlayerController> OwningController;
	TWeakObjectPtr<UInv_InventoryGrid> BoundInventoryGrid;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TSubclassOf<UInv_InventoryBase> InventoryMenuClass;

	UPROPERTY()
	TObjectPtr<UInv_InventoryBase> InventoryMenu;

	UPROPERTY(Replicated)
	bool bInventoryMenuOpen;

	void OpenInventoryMenu();
	void CloseInventoryMenu();
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMin = 80.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float DropSpawnDistanceMax = 180.0f;
	
	UPROPERTY(EditAnywhere, Category = "Inventory")
	float RelativeSpawnElevation = -70.0f;
};
