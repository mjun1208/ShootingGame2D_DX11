#ifndef WEAPON_INVENTORY_H
#define WEAPON_INVENTORY_H

enum class BulletType;

namespace WeaponInventory
{
	void Initialize();
	bool IsOwned(BulletType type);
	int GetOwnedCount();
	bool UnlockRandomWeapon(BulletType& out_type);
	bool ConsumeUnlockedWeapon(BulletType& out_type);
}

#endif // !WEAPON_INVENTORY_H
