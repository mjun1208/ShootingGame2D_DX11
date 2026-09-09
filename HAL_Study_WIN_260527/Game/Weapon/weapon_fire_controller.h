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
	void IncreaseProjectileCount(BulletType type);
	void MultiplyAttackSpeed(BulletType type, float multiplier);
	void MultiplyDamage(BulletType type, float multiplier);
	bool Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& target_position);
	void MaintainPassiveWeapons(const DirectX::XMFLOAT2& owner_position);
	void UpdateMultiShotBursts(float delta_time, const DirectX::XMFLOAT2& owner_position);
	void UpdateCooldowns(float delta_time);
} // namespace WeaponFireController

#endif // !WEAPON_FIRE_CONTROLLER_H
