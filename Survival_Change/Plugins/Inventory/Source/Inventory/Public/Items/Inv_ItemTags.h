#pragma once

#include "NativeGameplayTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon)
		}

		namespace BackPacks
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(BackPack)
		}
		
		namespace ChestRigs
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ChestRig)
		}
		
		namespace Helmets
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Helmet)
		}
		
		namespace ChestArmors
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ChestArmor)
		}
		
		namespace Pistols
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Pistol)
		}
		
		namespace Knifes
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Knife)
		}
	}

	namespace Consumables
	{
		namespace Poisons
		{
			namespace Red
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Small)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Large)
			}

			namespace Blue
			{
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Small)
				UE_DECLARE_GAMEPLAY_TAG_EXTERN(Large)
			}
		}
		
	}

	namespace Craftables
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(FireFernFruit)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(LuminDaisy)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(ScorchPetalBlossom)
	}
}

//物品属性
namespace Item
{
	namespace Stat
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strength)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mana)
	}
}