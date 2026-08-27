#include "weapon_upgrade_state.h"

#include "game_bullet.h"
#include "weapon_inventory.h"

#include <array>

namespace
{
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	std::array<WeaponUpgradeModifiers, WEAPON_COUNT> g_WeaponUpgrades{};

	WeaponUpgradeModifiers* GetOwnedUpgradeState(BulletType type)
	{
		const int index = static_cast<int>(type);
		return index >= 0 && index < WEAPON_COUNT &&
			WeaponInventory::IsOwned(type) ? &g_WeaponUpgrades[index] : nullptr;
	}
}

namespace WeaponUpgradeState
{
void Initialize()
{
	g_WeaponUpgrades.fill(WeaponUpgradeModifiers{});
}

const WeaponUpgradeModifiers& Get(BulletType type)
{
	const int index = static_cast<int>(type);
	return g_WeaponUpgrades[index >= 0 && index < WEAPON_COUNT ? index : 0];
}

void IncreaseProjectileCount(BulletType type)
{
	if (WeaponUpgradeModifiers* upgrades = GetOwnedUpgradeState(type))
	{
		++upgrades->AdditionalProjectileCount;
	}
}

void MultiplyAttackSpeed(BulletType type, float multiplier)
{
	if (multiplier <= 0.0f)
	{
		return;
	}
	if (WeaponUpgradeModifiers* upgrades = GetOwnedUpgradeState(type))
	{
		upgrades->AttackSpeedMultiplier *= multiplier;
	}
}

void MultiplyDamage(BulletType type, float multiplier)
{
	if (multiplier <= 0.0f)
	{
		return;
	}
	if (WeaponUpgradeModifiers* upgrades = GetOwnedUpgradeState(type))
	{
		upgrades->DamageMultiplier *= multiplier;
	}
}
}
