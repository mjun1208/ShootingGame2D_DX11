#ifndef WEAPON_FIRE_CONTROLLER_H
#define WEAPON_FIRE_CONTROLLER_H

#include <DirectXMath.h>

enum class BulletType;

namespace WeaponFireController
{
	void Initialize();
	void Finalize();
	void Clear();
	bool UnlockRandomWeapon();
	bool ConsumeUnlockedWeapon(BulletType& out_type);
	bool IsWeaponOwned(BulletType type);
	void IncreaseProjectileCount(BulletType type);
	void MultiplyAttackSpeed(BulletType type, float multiplier);
	void MultiplyDamage(BulletType type, float multiplier);
	bool Fire(
		const DirectX::XMFLOAT2& spawn_position,
		const DirectX::XMFLOAT2& target_position);
	void MaintainPassiveWeapons(const DirectX::XMFLOAT2& owner_position);
	void UpdateMultiShotBursts(
		float delta_time,
		const DirectX::XMFLOAT2& owner_position);
	void UpdateCooldowns(float delta_time);
}

#endif // !WEAPON_FIRE_CONTROLLER_H
