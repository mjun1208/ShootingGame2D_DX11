#include "weapon_shotgun.h"

#include "weapon_fire_strategy.h"

#include <cmath>
#include <cstdint>

namespace
{
	constexpr float DEGREES_TO_RADIANS = 0.01745329252f;
	constexpr std::uint32_t INITIAL_RANDOM_STATE = 0x51A7B00Bu;

	class ShotgunFireStrategy final : public WeaponVolleyFireStrategy
	{
	public:
		void Reset() override
		{
			m_RandomState = INITIAL_RANDOM_STATE;
		}

		bool Fire(const WeaponFireContext& context) override
		{
			return FireVolley(context);
		}

	protected:
		WeaponProjectileVariation MakeProjectileVariation(
			float centered_amount) override
		{
			WeaponProjectileVariation variation{};
			variation.CenteredAmount = std::copysign(
				centered_amount * centered_amount,
				centered_amount);
			variation.AngleOffset =
				(NextRandom01() * 2.0f - 1.0f) *
				1.5f * DEGREES_TO_RADIANS;
			variation.SpeedMultiplier = 0.88f + NextRandom01() * 0.17f;
			variation.LifetimeMultiplier = 0.86f + NextRandom01() * 0.18f;
			return variation;
		}

	private:
		float NextRandom01()
		{
			m_RandomState = m_RandomState * 1664525u + 1013904223u;
			return static_cast<float>(
				(m_RandomState >> 8) & 0x00ffffffu) /
				static_cast<float>(0x01000000u);
		}

		std::uint32_t m_RandomState{ INITIAL_RANDOM_STATE };
	};
}

namespace WeaponShotgun
{
std::unique_ptr<IWeaponFireStrategy> CreateFireStrategy()
{
	return std::make_unique<ShotgunFireStrategy>();
}
}
