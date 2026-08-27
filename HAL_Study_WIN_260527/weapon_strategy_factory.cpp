#include "weapon_strategy_factory.h"

#include "weapon_fire_strategy.h"
#include "weapon_magic_blade.h"
#include "weapon_shotgun.h"

namespace WeaponStrategyFactory
{
std::unique_ptr<IWeaponFireStrategy> Create(BulletType type)
{
	switch (type)
	{
	case BulletType::OrbitBlade:
		return CreateOrbitWeaponFireStrategy();
	case BulletType::Shotgun:
		return WeaponShotgun::CreateFireStrategy();
	case BulletType::MagicBlade:
		return WeaponMagicBlade::CreateFireStrategy();
	case BulletType::Fireball:
	case BulletType::Lightning:
	case BulletType::Ricochet:
	case BulletType::BezierHoming:
	case BulletType::Boomerang:
		return CreateDefaultWeaponFireStrategy();
	default:
		return nullptr;
	}
}
}
