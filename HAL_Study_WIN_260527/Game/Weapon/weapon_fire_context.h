#ifndef WEAPON_FIRE_CONTEXT_H
#define WEAPON_FIRE_CONTEXT_H

#include "game_bullet.h"
#include "projectile.h"

struct WeaponData;

struct WeaponFireContext
{
	BulletType Type;
	const WeaponData& Profile;
	DirectX::XMFLOAT2 SpawnPosition;
	DirectX::XMFLOAT2 AimDirection;
	float DamageMultiplier;

	cProjectileDesc BuildProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction,
	                                float speed_multiplier = 1.0f, float lifetime_multiplier = 1.0f) const;
	bool SubmitProjectile(const cProjectileDesc& desc) const;
	bool SpawnProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction,
	                     float speed_multiplier = 1.0f, float lifetime_multiplier = 1.0f) const;
};

bool FireWeaponVolley(const WeaponFireContext& context, bool shotgun = false);

#endif // !WEAPON_FIRE_CONTEXT_H
