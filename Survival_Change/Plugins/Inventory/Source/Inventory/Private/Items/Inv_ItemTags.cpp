#include "Items/Inv_ItemTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DEFINE_GAMEPLAY_TAG(Weapon, "GameItems.Equipment.Weapons.Weapon")
		}
		
		namespace BackPacks
		{
			UE_DEFINE_GAMEPLAY_TAG(BackPack, "GameItems.Equipment.BackPacks.BackPack")
		}
		
		namespace ChestRigs
		{
			UE_DEFINE_GAMEPLAY_TAG(ChestRig, "GameItems.Equipment.ChestRigs.ChestRig")
		}
		
		namespace Helmets
		{
			UE_DEFINE_GAMEPLAY_TAG(Helmet, "GameItems.Equipment.Helmets.Helmet")
		}
		
		namespace ChestArmors
		{
			UE_DEFINE_GAMEPLAY_TAG(ChestArmor, "GameItems.Equipment.ChestArmors.ChestArmor")
		}
		
		namespace Pistols
		{
			UE_DEFINE_GAMEPLAY_TAG(Pistol, "GameItems.Equipment.Pistols.Pistol")
		}
		
		namespace Knifes
		{
			UE_DEFINE_GAMEPLAY_TAG(Knife, "GameItems.Equipment.Knifes.Knife")
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
		UE_DEFINE_GAMEPLAY_TAG(Strength, "Item.Stat.Strength")
		UE_DEFINE_GAMEPLAY_TAG(Health, "Item.Stat.Health")
		UE_DEFINE_GAMEPLAY_TAG(Mana, "Item.Stat.Mana")
	}
}