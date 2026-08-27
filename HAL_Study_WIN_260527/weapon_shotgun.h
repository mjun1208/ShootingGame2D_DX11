#ifndef WEAPON_SHOTGUN_H
#define WEAPON_SHOTGUN_H

#include <memory>

class IWeaponFireStrategy;

namespace WeaponShotgun
{
	std::unique_ptr<IWeaponFireStrategy> CreateFireStrategy();
}

#endif // !WEAPON_SHOTGUN_H
