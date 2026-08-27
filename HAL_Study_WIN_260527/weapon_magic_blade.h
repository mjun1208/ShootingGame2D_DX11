#ifndef WEAPON_MAGIC_BLADE_H
#define WEAPON_MAGIC_BLADE_H

#include <memory>

class IWeaponFireStrategy;

namespace WeaponMagicBlade
{
	std::unique_ptr<IWeaponFireStrategy> CreateFireStrategy();
}

#endif // !WEAPON_MAGIC_BLADE_H
