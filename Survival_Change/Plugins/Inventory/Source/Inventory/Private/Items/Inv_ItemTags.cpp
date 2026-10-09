#include "Items/Inv_ItemTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DEFINE_GAMEPLAY_TAG(Axe, "GameItems.Equipment.Weapons.Axe")
			UE_DEFINE_GAMEPLAY_TAG(Sword, "GameItems.Equipment.Weapons.Sword")
		}
		
		namespace Cloaks
		{
			UE_DEFINE_GAMEPLAY_TAG(RedCloak, "GameItems.Equipment.Cloaks.RedCloak")
		}
		
		namespace Masks
		{
			UE_DEFINE_GAMEPLAY_TAG(SteelMask, "GameItems.Equipment.Masks.SteelMask")
		}

		namespace Slot
		{
			UE_DEFINE_GAMEPLAY_TAG(Weapon, "GameItems.Equipment.Slot.Weapon")
			UE_DEFINE_GAMEPLAY_TAG(Back, "GameItems.Equipment.Slot.Back")
			UE_DEFINE_GAMEPLAY_TAG(Face, "GameItems.Equipment.Slot.Face")
		}
	}
	namespace Consumables
	{
		namespace Poisons
		{
			namespace Red
			{
				UE_DEFINE_GAMEPLAY_TAG(Small, "GameItems.Consumables.Poisons.Red.Small")
				UE_DEFINE_GAMEPLAY_TAG(Large, "GameItems.Consumables.Poisons.Red.Large")
			}

			namespace Blue
			{
				UE_DEFINE_GAMEPLAY_TAG(Small, "GameItems.Consumables.Poisons.Blue.Small")
				UE_DEFINE_GAMEPLAY_TAG(Large, "GameItems.Consumables.Poisons.Blue.Large")
			}
		}
	}

	namespace Craftables
	{
		UE_DEFINE_GAMEPLAY_TAG(FireFernFruit, "GameItems.Craftables.FireFernFruit")
		UE_DEFINE_GAMEPLAY_TAG(LuminDaisy, "GameItems.Craftables.LuminDaisy")
		UE_DEFINE_GAMEPLAY_TAG(ScorchPetalBlossom, "GameItems.Craftables.ScorchPetalBlossom")
	}
}

namespace Item
{
	namespace Stat
	{
		UE_DEFINE_GAMEPLAY_TAG(AttackPower, "Item.Stat.AttackPower")
		UE_DEFINE_GAMEPLAY_TAG(Strength, "Item.Stat.Strength")
		UE_DEFINE_GAMEPLAY_TAG(Health, "Item.Stat.Health")
		UE_DEFINE_GAMEPLAY_TAG(Mana, "Item.Stat.Mana")
	}
}