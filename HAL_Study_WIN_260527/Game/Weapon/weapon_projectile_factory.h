#ifndef WEAPON_PROJECTILE_FACTORY_H
#define WEAPON_PROJECTILE_FACTORY_H

#include "game_bullet.h"
#include "projectile.h"

struct WeaponData;

namespace WeaponProjectileFactory
{
	void Initialize();
	void Finalize();
	int GetTextureID(BulletType type);
	cProjectileDesc Build(BulletType type, const WeaponData& profile, const DirectX::XMFLOAT2& spawn_position,
	                      const DirectX::XMFLOAT2& normalized_direction, float damage_multiplier,
	                      float speed_multiplier = 1.0f, float lifetime_multiplier = 1.0f);
	bool Submit(const cProjectileDesc& desc);
} // namespace WeaponProjectileFactory

#endif // !WEAPON_PROJECTILE_FACTORY_H
