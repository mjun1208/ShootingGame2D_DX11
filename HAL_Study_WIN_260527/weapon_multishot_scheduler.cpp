#include "weapon_multishot_scheduler.h"

#include "game_bullet.h"

#include <algorithm>
#include <array>

namespace
{
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	constexpr float FOLLOW_UP_INTERVAL = 0.10f;

	struct WeaponBurstState
	{
		int RemainingShots{ 0 };
		float TimeUntilNextShot{ 0.0f };
		DirectX::XMFLOAT2 SpawnOffset{};
		DirectX::XMFLOAT2 AimDirection{ 1.0f, 0.0f };
	};

	std::array<WeaponBurstState, WEAPON_COUNT> g_Bursts{};

	bool IsConcreteWeaponType(BulletType type)
	{
		const int index = static_cast<int>(type);
		return index >= 0 && index < WEAPON_COUNT;
	}
}

namespace WeaponMultiShotScheduler
{
void Reset()
{
	g_Bursts.fill(WeaponBurstState{});
}

bool IsActive(BulletType type)
{
	return IsConcreteWeaponType(type) &&
		g_Bursts[static_cast<int>(type)].RemainingShots > 0;
}

void Schedule(
	BulletType type,
	int shot_count,
	const DirectX::XMFLOAT2& spawn_offset,
	const DirectX::XMFLOAT2& aim_direction)
{
	if (!IsConcreteWeaponType(type) || shot_count <= 0)
	{
		return;
	}
	WeaponBurstState& burst = g_Bursts[static_cast<int>(type)];
	burst.RemainingShots = shot_count;
	burst.TimeUntilNextShot = FOLLOW_UP_INTERVAL;
	burst.SpawnOffset = spawn_offset;
	burst.AimDirection = aim_direction;
}

void Update(
	float delta_time,
	const DirectX::XMFLOAT2& owner_position,
	WeaponMultiShotFireCallback fire_callback)
{
	if (!fire_callback)
	{
		return;
	}
	const float safe_delta_time = std::max(delta_time, 0.0f);
	for (int i = 0; i < WEAPON_COUNT; ++i)
	{
		WeaponBurstState& burst = g_Bursts[i];
		if (burst.RemainingShots <= 0)
		{
			continue;
		}

		burst.TimeUntilNextShot -= safe_delta_time;
		while (burst.RemainingShots > 0 &&
			burst.TimeUntilNextShot <= 0.0f)
		{
			const DirectX::XMFLOAT2 spawn_position = {
				owner_position.x + burst.SpawnOffset.x,
				owner_position.y + burst.SpawnOffset.y,
			};
			fire_callback(
				static_cast<BulletType>(i),
				spawn_position,
				burst.AimDirection);
			--burst.RemainingShots;
			burst.TimeUntilNextShot += FOLLOW_UP_INTERVAL;
		}
	}
}
}
