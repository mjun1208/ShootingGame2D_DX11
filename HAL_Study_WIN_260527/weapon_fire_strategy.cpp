#include "weapon_fire_strategy.h"

#include "projectile.h"
#include "weapon_data.h"
#include "weapon_projectile_factory.h"

#include <cmath>

namespace
{
	constexpr float TWO_PI = 6.28318530718f;
	constexpr float DEGREES_TO_RADIANS = TWO_PI / 360.0f;

	class DefaultWeaponFireStrategy : public WeaponVolleyFireStrategy
	{
	public:
		bool Fire(const WeaponFireContext& context) override
		{
			if (context.GetProfile().VolleyCount > 1)
			{
				return FireVolley(context);
			}
			return context.SpawnProjectile(
				context.GetSpawnPosition(), context.GetAimDirection());
		}
	};

	class OrbitWeaponFireStrategy final : public DefaultWeaponFireStrategy
	{
	public:
		void OnDeselected(const WeaponData& profile) override
		{
			ProjectileSystem_DeactivateGroup(profile.AliveLimitGroup);
		}
	};
}

WeaponFireContext::WeaponFireContext(
	BulletType type,
	const WeaponData& profile,
	const DirectX::XMFLOAT2& spawn_position,
	const DirectX::XMFLOAT2& aim_direction,
	float damage_multiplier)
	: m_Type(type),
	  m_Profile(profile),
	  m_SpawnPosition(spawn_position),
	  m_AimDirection(aim_direction),
	  m_DamageMultiplier(damage_multiplier)
{
}

BulletType WeaponFireContext::GetType() const
{
	return m_Type;
}

const WeaponData& WeaponFireContext::GetProfile() const
{
	return m_Profile;
}

const DirectX::XMFLOAT2& WeaponFireContext::GetSpawnPosition() const
{
	return m_SpawnPosition;
}

const DirectX::XMFLOAT2& WeaponFireContext::GetAimDirection() const
{
	return m_AimDirection;
}

cProjectileDesc WeaponFireContext::BuildProjectile(
	const DirectX::XMFLOAT2& position,
	const DirectX::XMFLOAT2& direction,
	float speed_multiplier,
	float lifetime_multiplier) const
{
	return WeaponProjectileFactory::Build(
		m_Type,
		m_Profile,
		position,
		direction,
		m_DamageMultiplier,
		speed_multiplier,
		lifetime_multiplier);
}

bool WeaponFireContext::SubmitProjectile(const cProjectileDesc& desc) const
{
	return WeaponProjectileFactory::Submit(desc);
}

bool WeaponFireContext::SpawnProjectile(
	const DirectX::XMFLOAT2& position,
	const DirectX::XMFLOAT2& direction,
	float speed_multiplier,
	float lifetime_multiplier) const
{
	return SubmitProjectile(BuildProjectile(
		position, direction, speed_multiplier, lifetime_multiplier));
}

WeaponProjectileVariation WeaponVolleyFireStrategy::MakeProjectileVariation(
	float centered_amount)
{
	WeaponProjectileVariation variation{};
	variation.CenteredAmount = centered_amount;
	return variation;
}

bool WeaponVolleyFireStrategy::FireVolley(
	const WeaponFireContext& context)
{
	const WeaponData& profile = context.GetProfile();
	if (profile.VolleyCount <= 0)
	{
		return false;
	}

	const DirectX::XMFLOAT2& aim_direction = context.GetAimDirection();
	const float base_angle = std::atan2(aim_direction.y, aim_direction.x);
	const bool is_radial_volley = profile.VolleyArcDegrees >= 359.0f;
	const float volley_arc = profile.VolleyArcDegrees * DEGREES_TO_RADIANS;
	bool fired_any = false;

	for (int i = 0; i < profile.VolleyCount; ++i)
	{
		float angle = base_angle;
		float centered_amount = 0.0f;
		if (is_radial_volley)
		{
			angle += TWO_PI * static_cast<float>(i) /
				static_cast<float>(profile.VolleyCount);
		}
		else if (profile.VolleyCount > 1)
		{
			const float amount = static_cast<float>(i) /
				static_cast<float>(profile.VolleyCount - 1);
			centered_amount = amount * 2.0f - 1.0f;
		}

		const WeaponProjectileVariation variation =
			MakeProjectileVariation(centered_amount);
		if (!is_radial_volley)
		{
			angle += variation.CenteredAmount * volley_arc * 0.5f;
		}
		angle += variation.AngleOffset;

		const DirectX::XMFLOAT2 projectile_direction = {
			std::cos(angle),
			std::sin(angle),
		};
		const DirectX::XMFLOAT2 projectile_position = {
			context.GetSpawnPosition().x +
				projectile_direction.x * profile.VolleySpawnRadius,
			context.GetSpawnPosition().y +
				projectile_direction.y * profile.VolleySpawnRadius,
		};
		fired_any = context.SpawnProjectile(
			projectile_position,
			projectile_direction,
			variation.SpeedMultiplier,
			variation.LifetimeMultiplier) || fired_any;
	}

	if (fired_any)
	{
		OnVolleyFired(context);
	}
	return fired_any;
}

std::unique_ptr<IWeaponFireStrategy> CreateDefaultWeaponFireStrategy()
{
	return std::make_unique<DefaultWeaponFireStrategy>();
}

std::unique_ptr<IWeaponFireStrategy> CreateOrbitWeaponFireStrategy()
{
	return std::make_unique<OrbitWeaponFireStrategy>();
}
