#include "weapon_inventory.h"

#include "game_bullet.h"

#include <array>
#include <random>

namespace
{
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	std::array<bool, WEAPON_COUNT> g_OwnedWeapons{};
	std::mt19937 g_UnlockRandomGenerator{ std::random_device{}() };
	BulletType g_PendingUnlockedWeapon = BulletType::Count;

	bool IsConcreteWeaponType(BulletType type)
	{
		const int index = static_cast<int>(type);
		return index >= 0 && index < WEAPON_COUNT;
	}
}

namespace WeaponInventory
{
void Initialize()
{
	g_OwnedWeapons.fill(false);
	g_PendingUnlockedWeapon = BulletType::Count;
}

bool IsOwned(BulletType type)
{
	return IsConcreteWeaponType(type) &&
		g_OwnedWeapons[static_cast<int>(type)];
}

int GetOwnedCount()
{
	int owned_count = 0;
	for (bool is_owned : g_OwnedWeapons)
	{
		owned_count += is_owned ? 1 : 0;
	}
	return owned_count;
}

bool UnlockRandomWeapon(BulletType& out_type)
{
	std::array<int, WEAPON_COUNT> locked_weapon_indices{};
	int locked_weapon_count = 0;
	for (int index = 0; index < WEAPON_COUNT; ++index)
	{
		if (!g_OwnedWeapons[index])
		{
			locked_weapon_indices[locked_weapon_count++] = index;
		}
	}
	if (locked_weapon_count <= 0)
	{
		return false;
	}

	std::uniform_int_distribution<int> distribution(
		0, locked_weapon_count - 1);
	const int unlocked_index =
		locked_weapon_indices[distribution(g_UnlockRandomGenerator)];
	g_OwnedWeapons[unlocked_index] = true;
	out_type = static_cast<BulletType>(unlocked_index);
	g_PendingUnlockedWeapon = out_type;
	return true;
}

bool ConsumeUnlockedWeapon(BulletType& out_type)
{
	if (g_PendingUnlockedWeapon == BulletType::Count)
	{
		return false;
	}
	out_type = g_PendingUnlockedWeapon;
	g_PendingUnlockedWeapon = BulletType::Count;
	return true;
}
}
