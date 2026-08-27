#ifndef WEAPON_FIRE_STRATEGY_H
#define WEAPON_FIRE_STRATEGY_H

#include "game_bullet.h"
#include "projectile.h"

#include <memory>

struct WeaponData;

class WeaponFireContext final
{
public:
	WeaponFireContext(
		BulletType type,
		const WeaponData& profile,
		const DirectX::XMFLOAT2& spawn_position,
		const DirectX::XMFLOAT2& aim_direction,
		float damage_multiplier);

	BulletType GetType() const;
	const WeaponData& GetProfile() const;
	const DirectX::XMFLOAT2& GetSpawnPosition() const;
	const DirectX::XMFLOAT2& GetAimDirection() const;

	cProjectileDesc BuildProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		float speed_multiplier = 1.0f,
		float lifetime_multiplier = 1.0f) const;
	bool SubmitProjectile(const cProjectileDesc& desc) const;
	bool SpawnProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		float speed_multiplier = 1.0f,
		float lifetime_multiplier = 1.0f) const;

private:
	BulletType m_Type;
	const WeaponData& m_Profile;
	DirectX::XMFLOAT2 m_SpawnPosition;
	DirectX::XMFLOAT2 m_AimDirection;
	float m_DamageMultiplier;
};

class IWeaponFireStrategy
{
public:
	virtual ~IWeaponFireStrategy() = default;

	virtual void Reset() {}
	virtual bool Fire(const WeaponFireContext& context) = 0;
	virtual void OnDeselected(const WeaponData&) {}
};

struct WeaponProjectileVariation
{
	float CenteredAmount{ 0.0f };
	float AngleOffset{ 0.0f };
	float SpeedMultiplier{ 1.0f };
	float LifetimeMultiplier{ 1.0f };
};

// Template Method for radial and arc volleys. Specialized weapons only
// customize per-projectile variation and the post-fire presentation.
class WeaponVolleyFireStrategy : public IWeaponFireStrategy
{
protected:
	bool FireVolley(const WeaponFireContext& context);
	virtual WeaponProjectileVariation MakeProjectileVariation(
		float centered_amount);
	virtual void OnVolleyFired(const WeaponFireContext&) {}
};

std::unique_ptr<IWeaponFireStrategy> CreateDefaultWeaponFireStrategy();
std::unique_ptr<IWeaponFireStrategy> CreateOrbitWeaponFireStrategy();

#endif // !WEAPON_FIRE_STRATEGY_H
