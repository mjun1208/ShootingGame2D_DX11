#ifndef WEAPON_STRATEGY_REGISTRY_H
#define WEAPON_STRATEGY_REGISTRY_H

#include "game_bullet.h"
#include "weapon_fire_strategy.h"

#include <array>
#include <memory>

struct WeaponData;

class WeaponStrategyRegistry final
{
public:
	static constexpr std::size_t STRATEGY_COUNT =
		static_cast<std::size_t>(BulletType::Count);

	bool Initialize();
	void Finalize();
	void Reset();
	IWeaponFireStrategy* Find(BulletType type);
	void NotifyDeselected(BulletType type, const WeaponData& profile);

private:
	std::array<std::unique_ptr<IWeaponFireStrategy>, STRATEGY_COUNT>
		m_Strategies{};
};

#endif // !WEAPON_STRATEGY_REGISTRY_H
