#include "Types/Inv_GridTypes.h"
#include "Items/Inv_InventoryItem.h"

FInv_SlotAvailability::FInv_SlotAvailability(int32 Index, int32 Room, bool bItemAtIndex,
	UInv_InventoryItem* InventoryItem)
: Index(Index), AmountToFill(Room), bItemAtIndex(bItemAtIndex), Item(InventoryItem)
{
	
}
