#include "weapon_strategy_registry.h"

#include "weapon_data.h"
#include "weapon_strategy_factory.h"

bool WeaponStrategyRegistry::Initialize()
{
	Finalize();
	static_assert(STRATEGY_COUNT == WeaponGameData::WEAPON_COUNT);
	for (std::size_t i = 0; i < m_Strategies.size(); ++i)
	{
		m_Strategies[i] = WeaponStrategyFactory::Create(
			static_cast<BulletType>(i));
		if (!m_Strategies[i])
		{
			Finalize();
			return false;
		}
	}
	return true;
}

void WeaponStrategyRegistry::Finalize()
{
	for (auto& strategy : m_Strategies)
	{
		strategy.reset();
	}
}

void WeaponStrategyRegistry::Reset()
{
	for (auto& strategy : m_Strategies)
	{
		if (strategy)
		{
			strategy->Reset();
		}
	}
}

IWeaponFireStrategy* WeaponStrategyRegistry::Find(BulletType type)
{
	const int index = static_cast<int>(type);
	if (index < 0 || index >= static_cast<int>(m_Strategies.size()))
	{
		return nullptr;
	}
	return m_Strategies[static_cast<std::size_t>(index)].get();
}

void WeaponStrategyRegistry::NotifyDeselected(
	BulletType type,
	const WeaponData& profile)
{
	IWeaponFireStrategy* strategy = Find(type);
	if (strategy)
	{
		strategy->OnDeselected(profile);
	}
}
