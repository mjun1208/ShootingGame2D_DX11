#include "weapon_fire_controller.h"

#include "game_bullet.h"
#include "game_data_manager.h"
#include "projectile.h"
#include "weapon_audio.h"
#include "weapon_data.h"
#include "weapon_fire_strategy.h"
#include "weapon_inventory.h"
#include "weapon_multishot_scheduler.h"
#include "weapon_strategy_registry.h"
#include "weapon_upgrade_state.h"

#include <cmath>

namespace
{
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	static_assert(WEAPON_COUNT == static_cast<int>(WeaponGameData::WEAPON_COUNT));
	constexpr float OWNED_WEAPON_LATERAL_OFFSET = 18.0f;
	float g_FireCooldowns[WEAPON_COUNT]{};
	WeaponStrategyRegistry g_StrategyRegistry;

	const WeaponData& GetWeaponData(BulletType type)
	{
		return GameDataManager::GetInstance()
			.GetWeaponGameData()
			.Get(static_cast<std::size_t>(type));
	}

	bool IsConcreteWeaponType(BulletType type)
	{
		const int index = static_cast<int>(type);
		return index >= 0 && index < WEAPON_COUNT;
	}

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
		const WeaponData& orbit_blade = GetWeaponData(BulletType::OrbitBlade);
		ProjectileSystem_DeactivateGroup(orbit_blade.AliveLimitGroup);
		g_FireCooldowns[orbit_blade_index] = 0.0f;
	}

	WeaponData BuildEffectiveProfile(
		BulletType type,
		bool include_added_projectiles)
	{
		WeaponData profile = GetWeaponData(type);
		const WeaponUpgradeModifiers& upgrades =
			WeaponUpgradeState::Get(type);
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

	bool SubmitWeaponFire(
		BulletType type,
		const WeaponData& profile,
		const DirectX::XMFLOAT2& spawn_position,
		const DirectX::XMFLOAT2& normalized_direction)
	{
		IWeaponFireStrategy* strategy = g_StrategyRegistry.Find(type);
		if (!strategy)
		{
			return false;
		}
		const WeaponFireContext context(
			type,
			profile,
			spawn_position,
			normalized_direction,
			WeaponUpgradeState::Get(type).DamageMultiplier);
		return strategy->Fire(context);
	}

	bool FireMultiShotFollowUp(
		BulletType type,
		const DirectX::XMFLOAT2& spawn_position,
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

	bool FireWeapon(
		BulletType type,
		const DirectX::XMFLOAT2& spawn_position,
		const DirectX::XMFLOAT2& normalized_direction,
		const DirectX::XMFLOAT2& owner_position)
	{
		if (!IsConcreteWeaponType(type))
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
		const WeaponUpgradeModifiers& upgrades =
			WeaponUpgradeState::Get(type);
		const WeaponData profile = BuildEffectiveProfile(type, is_orbit_blade);
		if (is_orbit_blade &&
			ProjectileSystem_GetActiveGroupCount(profile.AliveLimitGroup) >=
				profile.VolleyCount)
		{
			return false;
		}
		if (!SubmitWeaponFire(
			type, profile, spawn_position, normalized_direction))
		{
			return false;
		}

		g_FireCooldowns[type_index] =
			profile.FireInterval / upgrades.AttackSpeedMultiplier;
		if (!is_orbit_blade && upgrades.AdditionalProjectileCount > 0)
		{
			const DirectX::XMFLOAT2 spawn_offset = {
				spawn_position.x - owner_position.x,
				spawn_position.y - owner_position.y,
			};
			WeaponMultiShotScheduler::Schedule(
				type,
				upgrades.AdditionalProjectileCount,
				spawn_offset,
				normalized_direction);
		}
		WeaponAudio::PlayFire(type);
		return true;
	}
}

namespace WeaponFireController
{
void Initialize()
{
	WeaponInventory::Initialize();
	WeaponUpgradeState::Initialize();
	ResetCooldowns();
	WeaponMultiShotScheduler::Reset();
	g_StrategyRegistry.Initialize();
}

void Finalize()
{
	g_StrategyRegistry.Finalize();
	ResetCooldowns();
	WeaponMultiShotScheduler::Reset();
}

void Clear()
{
	ResetCooldowns();
	WeaponMultiShotScheduler::Reset();
	g_StrategyRegistry.Reset();
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

bool ConsumeUnlockedWeapon(BulletType& out_type)
{
	return WeaponInventory::ConsumeUnlockedWeapon(out_type);
}

bool IsWeaponOwned(BulletType type)
{
	return WeaponInventory::IsOwned(type);
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

bool Fire(
	const DirectX::XMFLOAT2& spawn_position,
	const DirectX::XMFLOAT2& target_position)
{
	const DirectX::XMFLOAT2 direction = {
		target_position.x - spawn_position.x,
		target_position.y - spawn_position.y,
	};
	const float direction_length_sq =
		direction.x * direction.x + direction.y * direction.y;
	if (direction_length_sq <= 0.0001f)
	{
		return false;
	}

	const float inverse_direction_length =
		1.0f / std::sqrt(direction_length_sq);
	const DirectX::XMFLOAT2 normalized_direction = {
		direction.x * inverse_direction_length,
		direction.y * inverse_direction_length,
	};
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
		const WeaponData& profile = GetWeaponData(weapon_type);
		DirectX::XMFLOAT2 weapon_position = spawn_position;
		if (profile.VolleyCount <= 1)
		{
			const float lateral_offset =
				(static_cast<float>(owned_weapon_index) -
					static_cast<float>(owned_weapon_count - 1) * 0.5f) *
				OWNED_WEAPON_LATERAL_OFFSET;
			weapon_position.x += perpendicular.x * lateral_offset;
			weapon_position.y += perpendicular.y * lateral_offset;
		}
		const DirectX::XMFLOAT2 weapon_aim = {
			target_position.x - weapon_position.x,
			target_position.y - weapon_position.y,
		};
		const float weapon_aim_length_sq =
			weapon_aim.x * weapon_aim.x + weapon_aim.y * weapon_aim.y;
		DirectX::XMFLOAT2 weapon_direction = normalized_direction;
		if (weapon_aim_length_sq > 0.0001f)
		{
			const float inverse_weapon_aim_length =
				1.0f / std::sqrt(weapon_aim_length_sq);
			weapon_direction = {
				weapon_aim.x * inverse_weapon_aim_length,
				weapon_aim.y * inverse_weapon_aim_length,
			};
		}
		fired_any = FireWeapon(
			weapon_type,
			weapon_position,
			weapon_direction,
			spawn_position) || fired_any;
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
	FireWeapon(
		BulletType::OrbitBlade,
		owner_position,
		{ 1.0f, 0.0f },
		owner_position);
}

void UpdateMultiShotBursts(
	float delta_time,
	const DirectX::XMFLOAT2& owner_position)
{
	WeaponMultiShotScheduler::Update(
		delta_time,
		owner_position,
		FireMultiShotFollowUp);
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
}
