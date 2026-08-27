#ifndef WEAPON_STRATEGY_FACTORY_H
#define WEAPON_STRATEGY_FACTORY_H

#include "game_bullet.h"

#include <memory>

class IWeaponFireStrategy;

namespace WeaponStrategyFactory
{
	std::unique_ptr<IWeaponFireStrategy> Create(BulletType type);
}

#endif // !WEAPON_STRATEGY_FACTORY_H
