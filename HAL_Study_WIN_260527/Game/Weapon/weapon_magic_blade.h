#ifndef WEAPON_MAGIC_BLADE_H
#define WEAPON_MAGIC_BLADE_H

struct WeaponFireContext;

namespace WeaponMagicBlade
{
	void Reset();
	bool Fire(const WeaponFireContext& context);
}

#endif // !WEAPON_MAGIC_BLADE_H
