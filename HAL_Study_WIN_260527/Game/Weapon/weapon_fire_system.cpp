#include "game_bullet.h"
#include "Constants/weapon_constants.h"
#include "game_data_manager.h"
#include "math_utils.h"
#include "random_utils.h"
#include "projectile.h"
#include "weapon_audio.h"
#include "weapon_data.h"
#include "weapon_fire_controller.h"
#include "weapon_fire_context.h"
#include "weapon_inventory.h"
#include "weapon_magic_blade.h"
#include "weapon_multishot_scheduler.h"
#include "weapon_projectile_factory.h"
#include "weapon_upgrade_state.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	static_assert(WEAPON_COUNT == static_cast<int>(WeaponGameData::WEAPON_COUNT));

	std::array<float, WEAPON_COUNT> g_FireCooldowns{};

	void ResetCooldowns()
	{
		for (float& cooldown : g_FireCooldowns)
		{
			cooldown = 0.0f;
		}
	}

	void RefreshOwnedOrbitBladeFormation()
	{
		const int orbit_blade_index = static_cast<int>(BulletType::OrbitBlade);
		if (!WeaponInventory::IsOwned(BulletType::OrbitBlade))
		{
			return;
		}
		const WeaponData& orbit_blade = GetWeaponData(static_cast<std::size_t>(BulletType::OrbitBlade));
		ProjectileSystem_DeactivateGroup(orbit_blade.AliveLimitGroup);
		g_FireCooldowns[orbit_blade_index] = 0.0f;
	}

	WeaponData BuildEffectiveProfile(BulletType type, bool include_added_projectiles)
	{
		WeaponData profile = GetWeaponData(static_cast<std::size_t>(type));
		const WeaponUpgradeModifiers& upgrades = WeaponUpgradeState::Get(type);
		if (include_added_projectiles)
		{
			profile.VolleyCount += upgrades.AdditionalProjectileCount;
		}
		profile.OrbitAngularSpeed *= upgrades.AttackSpeedMultiplier;
		if (profile.MaxAlive > 0)
		{
			profile.MaxAlive += upgrades.AdditionalProjectileCount;
		}
		return profile;
	}

	bool SubmitWeaponFire(BulletType type, const WeaponData& profile, const DirectX::XMFLOAT2& spawn_position,
	                      const DirectX::XMFLOAT2& normalized_direction)
	{
		const WeaponFireContext context{ type, profile, spawn_position, normalized_direction,
		                                WeaponUpgradeState::Get(type).DamageMultiplier };
		switch (type)
		{
		case BulletType::Shotgun:
			return FireWeaponVolley(context, true);
		case BulletType::MagicBlade:
			return WeaponMagicBlade::Fire(context);
		default:
			return profile.VolleyCount > 1
			           ? FireWeaponVolley(context)
			           : context.SpawnProjectile(spawn_position, normalized_direction);
		}
	}

	bool FireMultiShotFollowUp(BulletType type, const DirectX::XMFLOAT2& spawn_position,
	                           const DirectX::XMFLOAT2& aim_direction)
	{
		WeaponData profile = BuildEffectiveProfile(type, false);
		profile.VolleyCount = 1;
		if (!SubmitWeaponFire(type, profile, spawn_position, aim_direction))
		{
			return false;
		}
		WeaponAudio::PlayFire(type);
		return true;
	}

	bool FireWeapon(BulletType type, const DirectX::XMFLOAT2& spawn_position,
	                const DirectX::XMFLOAT2& normalized_direction, const DirectX::XMFLOAT2& owner_position)
	{
		if (!GameBullet::IsValidWeaponType(type))
		{
			return false;
		}

		const int type_index = static_cast<int>(type);
		if (g_FireCooldowns[type_index] > 0.0f)
		{
			return false;
		}
		if (WeaponMultiShotScheduler::IsActive(type))
		{
			return false;
		}

		const bool is_orbit_blade = type == BulletType::OrbitBlade;
		const WeaponUpgradeModifiers& upgrades = WeaponUpgradeState::Get(type);
		const WeaponData profile = BuildEffectiveProfile(type, is_orbit_blade);
		if (is_orbit_blade && ProjectileSystem_GetActiveGroupCount(profile.AliveLimitGroup) >= profile.VolleyCount)
		{
			return false;
		}
		if (!SubmitWeaponFire(type, profile, spawn_position, normalized_direction))
		{
			return false;
		}

		g_FireCooldowns[type_index] = profile.FireInterval / upgrades.AttackSpeedMultiplier;
		if (!is_orbit_blade && upgrades.AdditionalProjectileCount > 0)
		{
			const DirectX::XMFLOAT2 spawn_offset = {
				spawn_position.x - owner_position.x,
				spawn_position.y - owner_position.y,
			};
			WeaponMultiShotScheduler::Schedule(type, upgrades.AdditionalProjectileCount, spawn_offset,
			                                   normalized_direction);
		}
		WeaponAudio::PlayFire(type);
		return true;
	}
} // namespace

namespace WeaponFireController
{
	void Initialize()
	{
		WeaponInventory::Initialize();
		WeaponUpgradeState::Initialize();
		ResetCooldowns();
		WeaponMultiShotScheduler::Reset();
		WeaponMagicBlade::Reset();
	}

	void Finalize()
	{
		WeaponMagicBlade::Reset();
		ResetCooldowns();
		WeaponMultiShotScheduler::Reset();
	}

	void Clear()
	{
		ResetCooldowns();
		WeaponMultiShotScheduler::Reset();
		WeaponMagicBlade::Reset();
	}

	bool UnlockRandomWeapon()
	{
		BulletType unlocked_type = BulletType::Count;
		if (!WeaponInventory::UnlockRandomWeapon(unlocked_type))
		{
			return false;
		}
		g_FireCooldowns[static_cast<int>(unlocked_type)] = 0.0f;
		return true;
	}

	void MultiplyDamage(BulletType type, float multiplier)
	{
		if (multiplier > 0.0f)
		{
			WeaponUpgradeState::MultiplyDamage(type, multiplier);
			if (type == BulletType::OrbitBlade)
			{
				RefreshOwnedOrbitBladeFormation();
			}
		}
	}

	void IncreaseProjectileCount(BulletType type)
	{
		WeaponUpgradeState::IncreaseProjectileCount(type);
		if (type == BulletType::OrbitBlade)
		{
			RefreshOwnedOrbitBladeFormation();
		}
	}

	void MultiplyAttackSpeed(BulletType type, float multiplier)
	{
		if (multiplier > 0.0f)
		{
			WeaponUpgradeState::MultiplyAttackSpeed(type, multiplier);
			if (type == BulletType::OrbitBlade)
			{
				RefreshOwnedOrbitBladeFormation();
			}
		}
	}

	bool Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& target_position)
	{
		const DirectX::XMFLOAT2 direction = {
			target_position.x - spawn_position.x,
			target_position.y - spawn_position.y,
		};
		const float direction_length_sq = LengthSquared(direction);
		if (direction_length_sq <= 0.0001f)
		{
			return false;
		}

		const DirectX::XMFLOAT2 normalized_direction = NormalizeOr(direction, { 0.0f, 0.0f });
		const DirectX::XMFLOAT2 perpendicular = {
			-normalized_direction.y,
			normalized_direction.x,
		};
		const int owned_weapon_count = WeaponInventory::GetOwnedCount();

		bool fired_any = false;
		int owned_weapon_index = 0;
		for (int i = 0; i < WEAPON_COUNT; ++i)
		{
			if (!WeaponInventory::IsOwned(static_cast<BulletType>(i)))
			{
				continue;
			}

			const BulletType weapon_type = static_cast<BulletType>(i);
			const WeaponData& profile = GetWeaponData(static_cast<std::size_t>(weapon_type));
			DirectX::XMFLOAT2 weapon_position = spawn_position;
			if (profile.VolleyCount <= 1)
			{
				const float lateral_offset =
				    (static_cast<float>(owned_weapon_index) - static_cast<float>(owned_weapon_count - 1) * 0.5f) *
				    WeaponConstants::Fire::OwnedWeaponLateralOffset;
				weapon_position.x += perpendicular.x * lateral_offset;
				weapon_position.y += perpendicular.y * lateral_offset;
			}
			const DirectX::XMFLOAT2 weapon_aim = {
				target_position.x - weapon_position.x,
				target_position.y - weapon_position.y,
			};
			const DirectX::XMFLOAT2 weapon_direction = NormalizeOr(weapon_aim, normalized_direction);
			fired_any = FireWeapon(weapon_type, weapon_position, weapon_direction, spawn_position) || fired_any;
			++owned_weapon_index;
		}
		return fired_any;
	}

	void MaintainPassiveWeapons(const DirectX::XMFLOAT2& owner_position)
	{
		if (!WeaponInventory::IsOwned(BulletType::OrbitBlade))
		{
			return;
		}
		FireWeapon(BulletType::OrbitBlade, owner_position, { 1.0f, 0.0f }, owner_position);
	}

	void UpdateMultiShotBursts(float delta_time, const DirectX::XMFLOAT2& owner_position)
	{
		WeaponMultiShotScheduler::Update(delta_time, owner_position);
	}

	void UpdateCooldowns(float delta_time)
	{
		for (float& cooldown : g_FireCooldowns)
		{
			if (cooldown > 0.0f)
			{
				cooldown -= delta_time;
			}
		}
	}
} // namespace WeaponFireController

namespace
{
	constexpr float TWO_PI = 6.28318530718f;
	constexpr float DEGREES_TO_RADIANS = TWO_PI / 360.0f;

} // namespace

cProjectileDesc WeaponFireContext::BuildProjectile(const DirectX::XMFLOAT2& position,
                                                   const DirectX::XMFLOAT2& direction, float speed_multiplier,
                                                   float lifetime_multiplier) const
{
	return WeaponProjectileFactory::Build(Type, Profile, position, direction, DamageMultiplier, speed_multiplier,
	                                      lifetime_multiplier);
}

bool WeaponFireContext::SubmitProjectile(const cProjectileDesc& desc) const
{
	return WeaponProjectileFactory::Submit(desc);
}

bool WeaponFireContext::SpawnProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction,
                                        float speed_multiplier, float lifetime_multiplier) const
{
	return SubmitProjectile(BuildProjectile(position, direction, speed_multiplier, lifetime_multiplier));
}

bool FireWeaponVolley(const WeaponFireContext& context, bool shotgun)
{
	const WeaponData& profile = context.Profile;
	if (profile.VolleyCount <= 0)
	{
		return false;
	}

	const DirectX::XMFLOAT2& aim_direction = context.AimDirection;
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
			angle += TWO_PI * static_cast<float>(i) / static_cast<float>(profile.VolleyCount);
		}
		else if (profile.VolleyCount > 1)
		{
			const float amount = static_cast<float>(i) / static_cast<float>(profile.VolleyCount - 1);
			centered_amount = amount * 2.0f - 1.0f;
		}

		float angle_offset = 0.0f;
		float speed_multiplier = 1.0f;
		float lifetime_multiplier = 1.0f;
		if (shotgun)
		{
			centered_amount = std::copysign(centered_amount * centered_amount, centered_amount);
			angle_offset = RandomSigned() * 1.5f * 0.01745329252f;
			speed_multiplier = 0.88f + Random01() * 0.17f;
			lifetime_multiplier = 0.86f + Random01() * 0.18f;
		}
		if (!is_radial_volley)
		{
			angle += centered_amount * volley_arc * 0.5f;
		}
		angle += angle_offset;

		const DirectX::XMFLOAT2 projectile_direction = {
			std::cos(angle),
			std::sin(angle),
		};
		const DirectX::XMFLOAT2 projectile_position = {
			context.SpawnPosition.x + projectile_direction.x * profile.VolleySpawnRadius,
			context.SpawnPosition.y + projectile_direction.y * profile.VolleySpawnRadius,
		};
		fired_any = context.SpawnProjectile(projectile_position, projectile_direction, speed_multiplier,
		                                    lifetime_multiplier) ||
		            fired_any;
	}

	return fired_any;
}


namespace
{
	constexpr int BURST_WEAPON_COUNT = static_cast<int>(BulletType::Count);

	struct WeaponBurstState
	{
		int RemainingShots{ 0 };
		float TimeUntilNextShot{ 0.0f };
		DirectX::XMFLOAT2 SpawnOffset{};
		DirectX::XMFLOAT2 AimDirection{ 1.0f, 0.0f };
	};

	std::array<WeaponBurstState, BURST_WEAPON_COUNT> g_Bursts{};

	bool IsBurstWeaponType(BulletType type)
	{
		const int index = static_cast<int>(type);
		return index >= 0 && index < BURST_WEAPON_COUNT;
	}
} // namespace

namespace WeaponMultiShotScheduler
{
	void Reset()
	{
		g_Bursts.fill(WeaponBurstState{});
	}

	bool IsActive(BulletType type)
	{
		return IsBurstWeaponType(type) && g_Bursts[static_cast<int>(type)].RemainingShots > 0;
	}

	void Schedule(BulletType type, int shot_count, const DirectX::XMFLOAT2& spawn_offset,
	              const DirectX::XMFLOAT2& aim_direction)
	{
		if (!IsBurstWeaponType(type) || shot_count <= 0)
		{
			return;
		}
		WeaponBurstState& burst = g_Bursts[static_cast<int>(type)];
		burst.RemainingShots = shot_count;
		burst.TimeUntilNextShot = WeaponConstants::Fire::FollowUpInterval;
		burst.SpawnOffset = spawn_offset;
		burst.AimDirection = aim_direction;
	}

	void Update(float delta_time, const DirectX::XMFLOAT2& owner_position)
	{
		delta_time = std::max(delta_time, 0.0f);
		for (int i = 0; i < BURST_WEAPON_COUNT; ++i)
		{
			WeaponBurstState& burst = g_Bursts[i];
			if (burst.RemainingShots <= 0)
			{
				continue;
			}

			burst.TimeUntilNextShot -= delta_time;
			while (burst.RemainingShots > 0 && burst.TimeUntilNextShot <= 0.0f)
			{
				const DirectX::XMFLOAT2 spawn_position = {
					owner_position.x + burst.SpawnOffset.x,
					owner_position.y + burst.SpawnOffset.y,
				};
				FireMultiShotFollowUp(static_cast<BulletType>(i), spawn_position, burst.AimDirection);
				--burst.RemainingShots;
				burst.TimeUntilNextShot += WeaponConstants::Fire::FollowUpInterval;
			}
		}
	}
} // namespace WeaponMultiShotScheduler
