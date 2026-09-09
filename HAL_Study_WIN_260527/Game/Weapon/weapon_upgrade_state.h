#ifndef WEAPON_UPGRADE_STATE_H
#define WEAPON_UPGRADE_STATE_H

enum class BulletType;

struct WeaponUpgradeModifiers
{
	int AdditionalProjectileCount{ 0 };
	float AttackSpeedMultiplier{ 1.0f };
	float DamageMultiplier{ 1.0f };
};

namespace WeaponUpgradeState
{
	void Initialize();
	const WeaponUpgradeModifiers& Get(BulletType type);
	void IncreaseProjectileCount(BulletType type);
	void MultiplyAttackSpeed(BulletType type, float multiplier);
	void MultiplyDamage(BulletType type, float multiplier);
} // namespace WeaponUpgradeState

#endif // !WEAPON_UPGRADE_STATE_H
