#pragma once

#include "NativeGameplayTags.h"

namespace GameItems
{
	namespace Equipment
	{
		namespace Weapons
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Axe)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Sword)
		}
		//披风、斗篷
		namespace Cloaks
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(RedCloak)
		}
		
		//面具
		namespace Masks
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(SteelMask)
		}

		//装备槽
		namespace Slot
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Back)
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Face)
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
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackPower)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Strength)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Health)
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Mana)
	}
}