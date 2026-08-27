#include "enemy_attack_pattern.h"

#include "Audio.h"
#include "collision.h"
#include "enemy.h"
#include "game_enemy.h"
#include "game_effect.h"
#include "procedural_map.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "trail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
	constexpr DirectX::XMFLOAT4 ENEMY_PROJECTILE_OUTLINE_COLOR{
		1.0f, 0.035f, 0.015f, 0.96f
	};
	constexpr float ENEMY_PROJECTILE_OUTLINE_THICKNESS = 1.0f;
	constexpr int ENEMY_CAPACITY = 512;
	constexpr int BONE_PROJECTILE_CAPACITY = 256;
	constexpr int DAGGER_PROJECTILE_CAPACITY = 128;
	constexpr int MAGE_PROJECTILE_CAPACITY = 128;
	constexpr int WARRIOR_STRIKE_CAPACITY = 128;
	constexpr int GROUND_HAZARD_CAPACITY = 64;

	constexpr float SLIME_DASH_TRIGGER_RANGE = 540.0f;
	constexpr float SLIME_DASH_MINIMUM_RANGE = 90.0f;
	constexpr float SLIME_DASH_DISTANCE = 300.0f;
	constexpr float SLIME_DASH_SPEED = 900.0f;
	constexpr float SLIME_DASH_TELEGRAPH_DURATION = 0.58f;
	constexpr float SLIME_DASH_RECOVERY_DURATION = 0.30f;
	constexpr float DASH_TRACE_STEP = 8.0f;

	constexpr float BAT_DASH_TRIGGER_RANGE = 520.0f;
	constexpr float BAT_DASH_MINIMUM_RANGE = 120.0f;
	constexpr float BAT_DASH_MAX_DISTANCE = 620.0f;
	constexpr float BAT_DASH_OVERSHOOT = 140.0f;
	constexpr float BAT_DASH_SPEED = 1500.0f;
	constexpr float BAT_DASH_WINDUP_DURATION = 0.45f;
	constexpr float BAT_DASH_RECOVERY_DURATION = 0.22f;

	constexpr float SKELETON_THROW_RANGE = 760.0f;
	constexpr float SKELETON_THROW_MINIMUM_RANGE = 220.0f;
	constexpr float SKELETON_THROW_WINDUP_DURATION = 0.34f;
	constexpr float BONE_PROJECTILE_SPEED = 430.0f;
	constexpr float BONE_PROJECTILE_MAX_DISTANCE = 900.0f;
	constexpr float BONE_PROJECTILE_DAMAGE = 8.0f;
	constexpr float BONE_PROJECTILE_RADIUS = 12.0f;
	constexpr float BONE_PROJECTILE_WIDTH = 48.0f;
	constexpr float BONE_PROJECTILE_HEIGHT = 24.0f;
	constexpr float BONE_PROJECTILE_SPIN_SPEED = 8.5f;

	constexpr float SKELETON_MAGE_CAST_RANGE = 780.0f;
	constexpr float SKELETON_MAGE_CAST_MINIMUM_RANGE = 240.0f;
	constexpr float SKELETON_MAGE_CAST_DURATION = 0.92f;
	constexpr float SKELETON_MAGE_SHOT_INTERVAL = 0.14f;
	constexpr float SKELETON_MAGE_RECOVERY_DURATION = 0.32f;
	constexpr int SKELETON_MAGE_VOLLEY_COUNT = 3;
	constexpr float MAGE_PROJECTILE_SPEED = 520.0f;
	constexpr float MAGE_PROJECTILE_MAX_DISTANCE = 920.0f;
	constexpr float MAGE_PROJECTILE_DAMAGE = 7.0f;
	constexpr float MAGE_PROJECTILE_RADIUS = 12.0f;
	constexpr float MAGE_PROJECTILE_SIZE = 68.0f;
	constexpr float MAGE_PROJECTILE_FRAME_TIME = 0.075f;
	constexpr int MAGE_PROJECTILE_FRAME_COUNT = 4;
	constexpr int MAGE_PROJECTILE_FRAME_COLUMNS = 2;

	constexpr float WARRIOR_ATTACK_TRIGGER_DISTANCE = 132.0f;
	constexpr float WARRIOR_ATTACK_WINDUP_DURATION = 0.30f;
	constexpr float WARRIOR_ATTACK_RECOVERY_DURATION = 0.42f;
	constexpr float WARRIOR_STRIKE_OFFSET = 62.0f;
	constexpr float WARRIOR_STRIKE_RADIUS = 54.0f;
	constexpr float WARRIOR_STRIKE_LIFETIME = 0.12f;
	constexpr float SKELETON_WARRIOR_STRIKE_DAMAGE = 10.0f;
	constexpr float ORC_WARRIOR_STRIKE_DAMAGE = 14.0f;
	constexpr float ORC_WARRIOR_CHASE_SPEED_SCALE = 1.42f;
	constexpr float ORC_WARRIOR_SIDESTEP_TRIGGER_DISTANCE = 480.0f;
	constexpr float ORC_WARRIOR_SIDESTEP_DURATION = 0.10f;
	constexpr float ORC_WARRIOR_SIDESTEP_SPEED = 760.0f;
	constexpr float ORC_WARRIOR_SIDESTEP_FORWARD_WEIGHT = 0.78f;
	constexpr float ORC_WARRIOR_SIDESTEP_SIDE_WEIGHT = 0.625f;
	constexpr int ORC_WARRIOR_SIDESTEP_COUNT = 2;
	constexpr int ORC_WARRIOR_DASH_TRAIL_COUNT = 2;
	constexpr float ORC_WARRIOR_DASH_TRAIL_DURATION = 0.22f;

	constexpr float RANGED_RETREAT_DISTANCE = 300.0f;
	constexpr float RANGED_APPROACH_DISTANCE = 520.0f;
	constexpr float RANGED_STRAFE_SPEED = 72.0f;

	constexpr float ROGUE_STALK_DISTANCE = 340.0f;
	constexpr float ROGUE_VANISH_DURATION = 0.72f;
	constexpr float ROGUE_STEALTH_MOVE_SPEED = 720.0f;
	constexpr float ROGUE_FLANK_DISTANCE = 150.0f;
	constexpr float ROGUE_BACKSTAB_WINDUP_DURATION = 0.72f;
	constexpr float ROGUE_STRIKE_OFFSET = 72.0f;
	constexpr float ROGUE_STRIKE_RADIUS = 52.0f;
	constexpr float ROGUE_STRIKE_DAMAGE = 6.0f;
	constexpr float ROGUE_RETREAT_DURATION = 1.05f;
	constexpr float ROGUE_STRAFE_SPEED = 190.0f;

	constexpr float SKELETON_ROGUE_THROW_RANGE = 620.0f;
	constexpr float SKELETON_ROGUE_THROW_MINIMUM_RANGE = 180.0f;
	constexpr float SKELETON_ROGUE_WINDUP_DURATION = 0.38f;
	constexpr float SKELETON_ROGUE_EVADE_DURATION = 0.34f;
	constexpr float SKELETON_ROGUE_IDEAL_DISTANCE = 390.0f;
	constexpr float SKELETON_ROGUE_INNER_DISTANCE = 315.0f;
	constexpr float SKELETON_ROGUE_OUTER_DISTANCE = 500.0f;
	constexpr float SKELETON_ROGUE_STRAFE_SPEED = 155.0f;
	constexpr float SKELETON_ROGUE_DAGGER_ANGLE_STEP = 0.14f;
	constexpr float DAGGER_PROJECTILE_SPEED = 560.0f;
	constexpr float DAGGER_PROJECTILE_MAX_DISTANCE = 820.0f;
	constexpr float DAGGER_PROJECTILE_DAMAGE = 6.0f;
	constexpr float DAGGER_PROJECTILE_RADIUS = 9.0f;
	constexpr float DAGGER_PROJECTILE_SIZE = 32.0f;

	constexpr float ORC_AXE_THROW_RANGE = 780.0f;
	constexpr float ORC_AXE_THROW_MINIMUM_RANGE = 240.0f;
	constexpr float ORC_AXE_THROW_WINDUP_DURATION = 0.48f;
	constexpr float ORC_AXE_PROJECTILE_SPEED = 500.0f;
	constexpr float ORC_AXE_PROJECTILE_MAX_DISTANCE = 920.0f;
	constexpr float ORC_AXE_PROJECTILE_DAMAGE = 9.0f;
	constexpr float ORC_AXE_PROJECTILE_RADIUS = 17.0f;
	constexpr float ORC_AXE_PROJECTILE_SIZE = 58.0f;
	constexpr float ORC_AXE_PROJECTILE_SPIN_SPEED = 10.5f;

	constexpr float SHAMAN_CAST_RANGE = 720.0f;
	constexpr float SHAMAN_CAST_DURATION = 0.82f;
	constexpr float SHAMAN_RECOVERY_DURATION = 0.34f;
	constexpr float SHAMAN_HAZARD_RADIUS = 92.0f;
	constexpr float SHAMAN_HAZARD_DAMAGE = 12.0f;
	constexpr float SHAMAN_HAZARD_LIFETIME = 0.30f;

	constexpr float CTHULHU_ROAM_MIN_DURATION = 1.85f;
	constexpr float CTHULHU_ROAM_MAX_DURATION = 2.55f;
	constexpr float CTHULHU_STRAFE_SPEED = 118.0f;
	constexpr float CTHULHU_CAST_DURATION = 1.02f;
	constexpr float CTHULHU_VANISH_DURATION = 0.48f;
	constexpr float CTHULHU_DASH_WINDUP_DURATION = 0.82f;
	constexpr float CTHULHU_DASH_SPEED = 1120.0f;
	constexpr float CTHULHU_DASH_RECOVERY_DURATION = 0.78f;
	constexpr float CTHULHU_ROOM_EDGE_PADDING = 142.0f;
	constexpr float CTHULHU_DASH_TELEGRAPH_WIDTH = 220.0f;
	constexpr float CTHULHU_ANIMATION_FRAME_DURATION = 0.09f;
	constexpr int CTHULHU_IDLE_ROW = 0;
	constexpr int CTHULHU_WALK_ROW = 1;
	constexpr int CTHULHU_FLY_ROW = 2;
	constexpr int CTHULHU_ATTACK_ONE_ROW = 3;
	constexpr int CTHULHU_ATTACK_TWO_ROW = 4;
	constexpr int CTHULHU_IDLE_FRAME_COUNT = 15;
	constexpr int CTHULHU_WALK_FRAME_COUNT = 12;
	constexpr int CTHULHU_FLY_FRAME_COUNT = 6;
	constexpr int CTHULHU_ATTACK_ONE_FRAME_COUNT = 7;
	constexpr int CTHULHU_ATTACK_TWO_FRAME_COUNT = 9;
	constexpr int CTHULHU_FRAME_WIDTH = 192;
	constexpr int CTHULHU_FRAME_HEIGHT = 112;

	enum class ActionState : std::uint8_t
	{
		None,
		SlimeCooldown,
		SlimeTelegraph,
		SlimeDash,
		SlimeRecovery,
		SkeletonCooldown,
		SkeletonWindup,
		SkeletonMageCooldown,
		SkeletonMageWindup,
		SkeletonMageVolley,
		SkeletonMageRecovery,
		BatCooldown,
		BatWindup,
		BatDash,
		BatRecovery,
		OrcCooldown,
		OrcWindup,
		RogueApproach,
		RogueVanish,
		RogueBackstabWindup,
		RogueBackstabStrike,
		RogueRetreat,
		SkeletonRogueCooldown,
		SkeletonRogueWindup,
		SkeletonRogueBackstep,
		WarriorChase,
		WarriorSidestep,
		WarriorWindup,
		WarriorRecovery,
		ShamanCooldown,
		ShamanCast,
		ShamanRecovery,
		CthulhuRoam,
		CthulhuCast,
		CthulhuVanish,
		CthulhuDashWindup,
		CthulhuDash,
		CthulhuRecovery,
	};

	struct WarriorDashTrailRuntime
	{
		DirectX::XMFLOAT2 Start{};
		DirectX::XMFLOAT2 End{};
		float TimeRemaining{ 0.0f };
	};

	struct EnemyPatternRuntime
	{
		MonsterType Type{ MonsterType::SkeletonBase };
		ActionState State{ ActionState::None };
		DirectX::XMFLOAT2 LockedDirection{ 1.0f, 0.0f };
		DirectX::XMFLOAT2 TelegraphStart{};
		DirectX::XMFLOAT2 TelegraphEnd{};
		DirectX::XMFLOAT2 TargetPosition{};
		float Timer{ 0.0f };
		float DashDistance{ 0.0f };
		float TelegraphWidth{ 0.0f };
		float AnimationElapsed{ 0.0f };
		float MovementTimer{ 0.0f };
		float StrafeDirection{ 1.0f };
		int MovementStep{ 0 };
		int VolleyShotsRemaining{ 0 };
		std::array<WarriorDashTrailRuntime,
			ORC_WARRIOR_DASH_TRAIL_COUNT> WarriorDashTrails{};
		std::uint32_t Sequence{ 0 };
		bool IsActive{ false };
	};

	struct BoneProjectile
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Travelled{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ BONE_PROJECTILE_SPIN_SPEED };
		bool IsActive{ false };
	};

	struct DaggerProjectile
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Travelled{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		float MaxDistance{ DAGGER_PROJECTILE_MAX_DISTANCE };
		float Damage{ DAGGER_PROJECTILE_DAMAGE };
		float Radius{ DAGGER_PROJECTILE_RADIUS };
		float Size{ DAGGER_PROJECTILE_SIZE };
		bool IsAxe{ false };
		bool IsActive{ false };
	};

	struct MageProjectile
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Travelled{ 0.0f };
		float Rotation{ 0.0f };
		float AnimationElapsed{ 0.0f };
		bool IsActive{ false };
	};

	struct WarriorStrike
	{
		DirectX::XMFLOAT2 Position{};
		float Elapsed{ 0.0f };
		float Damage{ SKELETON_WARRIOR_STRIKE_DAMAGE };
		float Radius{ WARRIOR_STRIKE_RADIUS };
		float Lifetime{ WARRIOR_STRIKE_LIFETIME };
		bool IsActive{ false };
	};

	struct GroundHazard
	{
		DirectX::XMFLOAT2 Position{};
		float Elapsed{ 0.0f };
		float Lifetime{ SHAMAN_HAZARD_LIFETIME };
		float Radius{ SHAMAN_HAZARD_RADIUS };
		float Damage{ SHAMAN_HAZARD_DAMAGE };
		bool IsActive{ false };
	};

	std::array<EnemyPatternRuntime, ENEMY_CAPACITY> g_EnemyPatterns{};
	std::array<BoneProjectile, BONE_PROJECTILE_CAPACITY> g_BoneProjectiles{};
	std::array<DaggerProjectile, DAGGER_PROJECTILE_CAPACITY> g_DaggerProjectiles{};
	std::array<MageProjectile, MAGE_PROJECTILE_CAPACITY> g_MageProjectiles{};
	std::array<WarriorStrike, WARRIOR_STRIKE_CAPACITY> g_WarriorStrikes{};
	std::array<GroundHazard, GROUND_HAZARD_CAPACITY> g_GroundHazards{};
	std::uint32_t g_RandomSeed = 0x51A1D45Eu;
	int g_BoneTextureID = TEXTURE_INVALID_ID;
	int g_DaggerTextureID = TEXTURE_INVALID_ID;
	int g_AxeTextureID = TEXTURE_INVALID_ID;
	int g_MageProjectileTextureID = TEXTURE_INVALID_ID;
	int g_WarningTextureID = TEXTURE_INVALID_ID;
	int g_GroundEffectTextureID = TEXTURE_INVALID_ID;
	int g_BoneThrowAudioID = -1;
	std::array<int, SKELETON_MAGE_VOLLEY_COUNT> g_MageFireAudioIDs{
		-1, -1, -1
	};
	int g_WarriorSlashAudioID = -1;
	int g_BatDashAudioID = -1;
	int g_ShamanCastAudioID = -1;

	std::uint32_t Hash32(std::uint32_t value)
	{
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		value ^= value >> 16;
		return value;
	}

	bool IsValidEnemyID(int enemy_id)
	{
		return enemy_id >= 0 && enemy_id < ENEMY_CAPACITY;
	}

	bool IsBoneThrower(MonsterType type)
	{
		return type == MonsterType::SkeletonBase;
	}

	bool IsOrcRogue(MonsterType type)
	{
		return type == MonsterType::OrcRogue;
	}

	bool IsWarriorType(MonsterType type)
	{
		return type == MonsterType::SkeletonWarrior ||
			type == MonsterType::OrcWarrior;
	}

	float GetRandomDuration(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		float minimum,
		float maximum)
	{
		const std::uint32_t value = Hash32(
			g_RandomSeed ^
			static_cast<std::uint32_t>(enemy_id + 1) * 0x9e3779b9u ^
			++runtime.Sequence * 0x85ebca6bu);
		const float amount = static_cast<float>(value & 0xffffu) / 65535.0f;
		return minimum + (maximum - minimum) * amount;
	}

	float GetDistanceSquared(
		const DirectX::XMFLOAT2& first,
		const DirectX::XMFLOAT2& second)
	{
		const float dx = second.x - first.x;
		const float dy = second.y - first.y;
		return dx * dx + dy * dy;
	}

	DirectX::XMFLOAT2 GetDirection(
		const DirectX::XMFLOAT2& origin,
		const DirectX::XMFLOAT2& target)
	{
		const float dx = target.x - origin.x;
		const float dy = target.y - origin.y;
		const float length_squared = dx * dx + dy * dy;
		if (length_squared <= 0.0001f)
		{
			return { 1.0f, 0.0f };
		}
		const float inverse_length = 1.0f / std::sqrt(length_squared);
		return { dx * inverse_length, dy * inverse_length };
	}

	DirectX::XMFLOAT2 GetAttackOrigin(const cEnemy& enemy)
	{
		return enemy.GetMapCollisionCenter();
	}

	DirectX::XMFLOAT2 RotateDirection(
		const DirectX::XMFLOAT2& direction,
		float angle)
	{
		const float sine = std::sin(angle);
		const float cosine = std::cos(angle);
		return {
			direction.x * cosine - direction.y * sine,
			direction.x * sine + direction.y * cosine,
		};
	}

	void UpdateRangedMovement(
		int enemy_id,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		const float distance_squared = GetDistanceSquared(
			enemy.GetPosition(), player_position);
		if (distance_squared <
			RANGED_RETREAT_DISTANCE * RANGED_RETREAT_DISTANCE)
		{
			enemy.Update(delta_time, player_position, -1.05f);
			return;
		}
		if (distance_squared >
			RANGED_APPROACH_DISTANCE * RANGED_APPROACH_DISTANCE)
		{
			enemy.Update(delta_time, player_position, 0.72f);
			return;
		}

		enemy.Update(delta_time, player_position, 0.0f);
		const DirectX::XMFLOAT2 direction = GetDirection(
			enemy.GetPosition(), player_position);
		const float strafe_sign = (enemy_id & 1) == 0 ? 1.0f : -1.0f;
		enemy.ApplySeparation({
			-direction.y * RANGED_STRAFE_SPEED * strafe_sign * delta_time,
			direction.x * RANGED_STRAFE_SPEED * strafe_sign * delta_time,
		});
	}

	void UpdateSkeletonRogueMovement(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		const DirectX::XMFLOAT2 position = enemy.GetPosition();
		const float distance_squared = GetDistanceSquared(
			position, player_position);
		const float distance = std::sqrt(std::max(distance_squared, 0.0001f));

		float radial_speed_scale = 0.0f;
		if (distance < SKELETON_ROGUE_INNER_DISTANCE)
		{
			radial_speed_scale = -1.25f;
		}
		else if (distance > SKELETON_ROGUE_OUTER_DISTANCE)
		{
			radial_speed_scale = 1.05f;
		}
		else
		{
			radial_speed_scale = std::clamp(
				(distance - SKELETON_ROGUE_IDEAL_DISTANCE) / 150.0f,
				-0.42f,
				0.42f);
		}
		enemy.Update(delta_time, player_position, radial_speed_scale);

		runtime.MovementTimer -= delta_time;
		if (runtime.MovementTimer <= 0.0f)
		{
			runtime.StrafeDirection *= -1.0f;
			runtime.MovementTimer = GetRandomDuration(
				enemy_id, runtime, 1.15f, 2.25f);
		}

		const DirectX::XMFLOAT2 direction = GetDirection(
			enemy.GetPosition(), player_position);
		const DirectX::XMFLOAT2 strafe_direction{
			-direction.y * runtime.StrafeDirection,
			direction.x * runtime.StrafeDirection,
		};
		const DirectX::XMFLOAT2 before_strafe = enemy.GetPosition();
		const float intended_distance =
			SKELETON_ROGUE_STRAFE_SPEED * delta_time;
		enemy.ApplySeparation({
			strafe_direction.x * intended_distance,
			strafe_direction.y * intended_distance,
		});

		const DirectX::XMFLOAT2 after_strafe = enemy.GetPosition();
		const float travelled_along_strafe =
			(after_strafe.x - before_strafe.x) * strafe_direction.x +
			(after_strafe.y - before_strafe.y) * strafe_direction.y;
		if (intended_distance > 0.1f &&
			travelled_along_strafe < intended_distance * 0.18f)
		{
			runtime.StrafeDirection *= -1.0f;
			runtime.MovementTimer = GetRandomDuration(
				enemy_id, runtime, 0.75f, 1.35f);
		}
	}

	DirectX::XMFLOAT2 TraceDashEnd(
		const DirectX::XMFLOAT2& start,
		const DirectX::XMFLOAT2& direction,
		float distance,
		float radius)
	{
		DirectX::XMFLOAT2 traced_position = start;
		float travelled = 0.0f;
		while (travelled < distance)
		{
			const float step = std::min(
				DASH_TRACE_STEP,
				distance - travelled);
			const DirectX::XMFLOAT2 next_position{
				traced_position.x + direction.x * step,
				traced_position.y + direction.y * step,
			};
			if (!ProceduralMap_IsSegmentWalkable(
				traced_position, next_position, radius))
			{
				break;
			}
			traced_position = next_position;
			travelled += step;
		}
		return traced_position;
	}

	bool SpawnBoneProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		int enemy_id)
	{
		for (BoneProjectile& projectile : g_BoneProjectiles)
		{
			if (projectile.IsActive)
			{
				continue;
			}

			projectile = BoneProjectile{};
			projectile.Position = position;
			projectile.Velocity = {
				direction.x * BONE_PROJECTILE_SPEED,
				direction.y * BONE_PROJECTILE_SPEED,
			};
			projectile.Rotation = std::atan2(direction.y, direction.x);
			projectile.AngularVelocity = (enemy_id & 1) == 0 ?
				BONE_PROJECTILE_SPIN_SPEED : -BONE_PROJECTILE_SPIN_SPEED;
			projectile.IsActive = true;
			if (g_BoneThrowAudioID >= 0)
			{
				PlayAudio(g_BoneThrowAudioID);
			}
			return true;
		}
		return false;
	}

	bool SpawnMageProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		int volley_index)
	{
		for (MageProjectile& projectile : g_MageProjectiles)
		{
			if (projectile.IsActive)
			{
				continue;
			}

			projectile = MageProjectile{};
			projectile.Position = position;
			projectile.Velocity = {
				direction.x * MAGE_PROJECTILE_SPEED,
				direction.y * MAGE_PROJECTILE_SPEED,
			};
			projectile.Rotation = std::atan2(direction.y, direction.x) +
				DirectX::XM_PI;
			projectile.IsActive = true;
			const int safe_volley_index = std::clamp(
				volley_index, 0, SKELETON_MAGE_VOLLEY_COUNT - 1);
			const int fire_audio_id = g_MageFireAudioIDs[safe_volley_index];
			if (fire_audio_id >= 0)
			{
				PlayAudio(fire_audio_id);
			}
			return true;
		}
		return false;
	}

	bool SpawnWarriorStrike(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		MonsterType type)
	{
		for (WarriorStrike& strike : g_WarriorStrikes)
		{
			if (strike.IsActive)
			{
				continue;
			}

			strike = WarriorStrike{};
			strike.Position = position;
			strike.Damage = type == MonsterType::OrcWarrior ?
				ORC_WARRIOR_STRIKE_DAMAGE : SKELETON_WARRIOR_STRIKE_DAMAGE;
			strike.IsActive = true;

			const float effect_scale = type == MonsterType::OrcWarrior ?
				1.14f : 1.0f;
			cGameEffectManager::GetInstance().Play(
				GameEffectType::EnemyWarriorSlash,
				position,
				effect_scale,
				{ 1.0f, 0.05f, 0.02f, 1.0f },
				std::atan2(direction.y, direction.x));
			if (g_WarriorSlashAudioID >= 0)
			{
				PlayAudio(g_WarriorSlashAudioID);
			}
			return true;
		}
		return false;
	}

	bool SpawnRogueStrike(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		int stab_index)
	{
		for (WarriorStrike& strike : g_WarriorStrikes)
		{
			if (strike.IsActive)
			{
				continue;
			}

			strike = WarriorStrike{};
			strike.Position = position;
			strike.Damage = ROGUE_STRIKE_DAMAGE;
			strike.Radius = ROGUE_STRIKE_RADIUS;
			strike.Lifetime = 0.10f;
			strike.IsActive = true;

			const float angle = std::atan2(direction.y, direction.x) +
				(stab_index == 0 ? -0.10f : 0.10f);
			cGameEffectManager::GetInstance().Play(
				GameEffectType::DashSlashHitCut,
				position,
				0.62f,
				stab_index == 0 ?
					DirectX::XMFLOAT4{ 0.88f, 0.12f, 0.05f, 0.94f } :
					DirectX::XMFLOAT4{ 1.0f, 0.38f, 0.08f, 0.98f },
				angle);
			if (g_WarriorSlashAudioID >= 0)
			{
				PlayAudio(g_WarriorSlashAudioID);
			}
			return true;
		}
		return false;
	}

	void PlayRogueSmoke(const DirectX::XMFLOAT2& position)
	{
		cGameEffectManager::GetInstance().Play(
			GameEffectType::SmokePoof,
			position,
			0.74f,
			{ 0.34f, 0.12f, 0.10f, 0.92f });
	}

	void BeginOrcWarriorDashTrail(
		EnemyPatternRuntime& runtime,
		const DirectX::XMFLOAT2& position)
	{
		WarriorDashTrailRuntime& trail = runtime.WarriorDashTrails[
			static_cast<std::size_t>(runtime.MovementStep) %
			ORC_WARRIOR_DASH_TRAIL_COUNT];
		trail.Start = position;
		trail.End = position;
		trail.TimeRemaining = ORC_WARRIOR_DASH_TRAIL_DURATION;
		if (g_BatDashAudioID >= 0)
		{
			PlayAudio(g_BatDashAudioID);
		}
	}

	DirectX::XMFLOAT2 GetOrcWarriorSidestepDirection(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& player_position,
		float strafe_direction)
	{
		const DirectX::XMFLOAT2 forward = GetDirection(
			position, player_position);
		return {
			forward.x * ORC_WARRIOR_SIDESTEP_FORWARD_WEIGHT -
				forward.y * strafe_direction * ORC_WARRIOR_SIDESTEP_SIDE_WEIGHT,
			forward.y * ORC_WARRIOR_SIDESTEP_FORWARD_WEIGHT +
				forward.x * strafe_direction * ORC_WARRIOR_SIDESTEP_SIDE_WEIGHT,
		};
	}

	void BeginOrcWarriorSidestep(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		const cEnemy& enemy)
	{
		runtime.State = ActionState::WarriorSidestep;
		runtime.Timer = ORC_WARRIOR_SIDESTEP_DURATION;
		runtime.MovementStep = 0;
		runtime.StrafeDirection = (Hash32(
			g_RandomSeed ^ static_cast<std::uint32_t>(enemy_id + 1) *
				0x9e3779b9u ^ ++runtime.Sequence) & 1u) == 0u ? -1.0f : 1.0f;
		BeginOrcWarriorDashTrail(runtime, enemy.GetPosition());
	}

	bool SpawnGroundHazard(const DirectX::XMFLOAT2& position)
	{
		for (GroundHazard& hazard : g_GroundHazards)
		{
			if (hazard.IsActive)
			{
				continue;
			}
			hazard = GroundHazard{};
			hazard.Position = position;
			hazard.IsActive = true;
			if (g_ShamanCastAudioID >= 0)
			{
				PlayAudio(g_ShamanCastAudioID);
			}
			return true;
		}
		return false;
	}

	bool SpawnDaggerProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction)
	{
		for (DaggerProjectile& projectile : g_DaggerProjectiles)
		{
			if (projectile.IsActive)
			{
				continue;
			}
			projectile = DaggerProjectile{};
			projectile.Position = position;
			projectile.Velocity = {
				direction.x * DAGGER_PROJECTILE_SPEED,
				direction.y * DAGGER_PROJECTILE_SPEED,
			};
			projectile.Rotation = std::atan2(direction.y, direction.x) +
				DirectX::XM_PIDIV4;
			projectile.IsActive = true;
			return true;
		}
		return false;
	}

	bool SpawnAxeProjectile(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		int enemy_id)
	{
		for (DaggerProjectile& projectile : g_DaggerProjectiles)
		{
			if (projectile.IsActive)
			{
				continue;
			}
			projectile = DaggerProjectile{};
			projectile.Position = position;
			projectile.Velocity = {
				direction.x * ORC_AXE_PROJECTILE_SPEED,
				direction.y * ORC_AXE_PROJECTILE_SPEED,
			};
			projectile.Rotation = std::atan2(direction.y, direction.x);
			projectile.AngularVelocity = (enemy_id & 1) == 0 ?
				ORC_AXE_PROJECTILE_SPIN_SPEED : -ORC_AXE_PROJECTILE_SPIN_SPEED;
			projectile.MaxDistance = ORC_AXE_PROJECTILE_MAX_DISTANCE;
			projectile.Damage = ORC_AXE_PROJECTILE_DAMAGE;
			projectile.Radius = ORC_AXE_PROJECTILE_RADIUS;
			projectile.Size = ORC_AXE_PROJECTILE_SIZE;
			projectile.IsAxe = true;
			projectile.IsActive = true;
			if (g_BatDashAudioID >= 0)
			{
				PlayAudio(g_BatDashAudioID);
			}
			return true;
		}
		return false;
	}

	void FireDaggerFan(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& aim_direction,
		float muzzle_offset)
	{
		for (int dagger_index = -1; dagger_index <= 1; ++dagger_index)
		{
			const DirectX::XMFLOAT2 direction = RotateDirection(
				aim_direction,
				static_cast<float>(dagger_index) *
					SKELETON_ROGUE_DAGGER_ANGLE_STEP);
			SpawnDaggerProjectile({
				position.x + direction.x * muzzle_offset,
				position.y + direction.y * muzzle_offset,
			}, direction);
		}
	}

	void BeginSlimeTelegraph(
		EnemyPatternRuntime& runtime,
		const cEnemy& enemy,
		const DirectX::XMFLOAT2& player_position)
	{
		runtime.State = ActionState::SlimeTelegraph;
		runtime.Timer = SLIME_DASH_TELEGRAPH_DURATION;
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.LockedDirection = GetDirection(
			runtime.TelegraphStart, player_position);
		runtime.TelegraphEnd = TraceDashEnd(
			runtime.TelegraphStart,
			runtime.LockedDirection,
			SLIME_DASH_DISTANCE,
			enemy.GetCollisionRadius());
		runtime.TelegraphWidth = enemy.GetCollisionRadius() * 2.25f;
	}

	void UpdateSlime(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::SlimeCooldown:
		{
			enemy.Update(delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <= SLIME_DASH_TRIGGER_RANGE * SLIME_DASH_TRIGGER_RANGE &&
				distance_squared >= SLIME_DASH_MINIMUM_RANGE * SLIME_DASH_MINIMUM_RANGE)
			{
				BeginSlimeTelegraph(runtime, enemy, player_position);
			}
			break;
		}

		case ActionState::SlimeTelegraph:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = TraceDashEnd(
				runtime.TelegraphStart,
				runtime.LockedDirection,
				SLIME_DASH_DISTANCE,
				enemy.GetCollisionRadius());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SlimeDash;
				runtime.Timer = SLIME_DASH_DISTANCE / SLIME_DASH_SPEED;
			}
			break;

		case ActionState::SlimeDash:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
				runtime.LockedDirection.x * SLIME_DASH_SPEED * movement_time,
				runtime.LockedDirection.y * SLIME_DASH_SPEED * movement_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SlimeRecovery;
				runtime.Timer = SLIME_DASH_RECOVERY_DURATION;
			}
			break;
		}

		case ActionState::SlimeRecovery:
			enemy.Update(delta_time, player_position, 0.20f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SlimeCooldown;
				runtime.Timer = GetRandomDuration(enemy_id, runtime, 3.2f, 4.8f);
			}
			break;

		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}

	void UpdateSkeleton(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::SkeletonCooldown:
		{
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <= SKELETON_THROW_RANGE * SKELETON_THROW_RANGE &&
				distance_squared >=
					SKELETON_THROW_MINIMUM_RANGE * SKELETON_THROW_MINIMUM_RANGE)
			{
				runtime.LockedDirection = GetDirection(
					GetAttackOrigin(enemy), player_position);
				runtime.State = ActionState::SkeletonWindup;
				runtime.Timer = SKELETON_THROW_WINDUP_DURATION;
			}
			break;
		}

		case ActionState::SkeletonWindup:
			enemy.Update(delta_time, player_position, 0.05f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 position = GetAttackOrigin(enemy);
				const float muzzle_offset = enemy.GetCollisionRadius() + 14.0f;
				SpawnBoneProjectile({
					position.x + runtime.LockedDirection.x * muzzle_offset,
					position.y + runtime.LockedDirection.y * muzzle_offset,
				}, runtime.LockedDirection, enemy_id);
				runtime.State = ActionState::SkeletonCooldown;
				runtime.Timer = GetRandomDuration(enemy_id, runtime, 3.4f, 4.8f);
			}
			break;

		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}

	void UpdateSkeletonMage(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position,
		float visual_bottom_offset)
	{
		switch (runtime.State)
		{
		case ActionState::SkeletonMageCooldown:
		{
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <=
					SKELETON_MAGE_CAST_RANGE * SKELETON_MAGE_CAST_RANGE &&
				distance_squared >= SKELETON_MAGE_CAST_MINIMUM_RANGE *
					SKELETON_MAGE_CAST_MINIMUM_RANGE)
			{
				runtime.TelegraphStart = GetAttackOrigin(enemy);
				runtime.LockedDirection = GetDirection(
					runtime.TelegraphStart, player_position);
				runtime.TelegraphEnd = TraceDashEnd(
					runtime.TelegraphStart,
					runtime.LockedDirection,
					MAGE_PROJECTILE_MAX_DISTANCE,
					MAGE_PROJECTILE_RADIUS);
				runtime.TelegraphWidth = 8.0f;
				const DirectX::XMFLOAT2 position = enemy.GetPosition();
				runtime.TargetPosition = {
					position.x,
					position.y + visual_bottom_offset,
				};
				runtime.State = ActionState::SkeletonMageWindup;
				runtime.Timer = SKELETON_MAGE_CAST_DURATION;
			}
			break;
		}

		case ActionState::SkeletonMageWindup:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.LockedDirection = GetDirection(
				runtime.TelegraphStart, player_position);
			runtime.TelegraphEnd = TraceDashEnd(
				runtime.TelegraphStart,
				runtime.LockedDirection,
				MAGE_PROJECTILE_MAX_DISTANCE,
				MAGE_PROJECTILE_RADIUS);
			const DirectX::XMFLOAT2 position = enemy.GetPosition();
			runtime.TargetPosition = {
				position.x,
				position.y + visual_bottom_offset,
			};
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SkeletonMageVolley;
				runtime.Timer = 0.0f;
				runtime.VolleyShotsRemaining = SKELETON_MAGE_VOLLEY_COUNT;
			}
			break;
		}

		case ActionState::SkeletonMageVolley:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f && runtime.VolleyShotsRemaining > 0)
			{
				const int volley_index = SKELETON_MAGE_VOLLEY_COUNT -
					runtime.VolleyShotsRemaining;
				SpawnMageProjectile(
					GetAttackOrigin(enemy), runtime.LockedDirection, volley_index);
				--runtime.VolleyShotsRemaining;
				if (runtime.VolleyShotsRemaining > 0)
				{
					runtime.Timer += SKELETON_MAGE_SHOT_INTERVAL;
				}
				else
				{
					runtime.State = ActionState::SkeletonMageRecovery;
					runtime.Timer = SKELETON_MAGE_RECOVERY_DURATION;
				}
			}
			break;

		case ActionState::SkeletonMageRecovery:
			enemy.Update(delta_time, player_position, 0.08f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SkeletonMageCooldown;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 3.2f, 4.5f);
			}
			break;

		default:
			runtime.State = ActionState::SkeletonMageCooldown;
			runtime.Timer = 1.0f;
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			break;
		}
	}

	void BeginBatWindup(
		EnemyPatternRuntime& runtime,
		const cEnemy& enemy,
		const DirectX::XMFLOAT2& player_position,
		float animation_elapsed)
	{
		const float distance = std::sqrt(GetDistanceSquared(
			enemy.GetPosition(), player_position));
		runtime.State = ActionState::BatWindup;
		runtime.Timer = BAT_DASH_WINDUP_DURATION;
		runtime.AnimationElapsed = animation_elapsed;
		runtime.DashDistance = std::min(
			BAT_DASH_MAX_DISTANCE, distance + BAT_DASH_OVERSHOOT);
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.LockedDirection = GetDirection(
			runtime.TelegraphStart, player_position);
		runtime.TelegraphEnd = TraceDashEnd(
			runtime.TelegraphStart,
			runtime.LockedDirection,
			runtime.DashDistance,
			enemy.GetCollisionRadius());
		runtime.TelegraphWidth = enemy.GetCollisionRadius() * 1.65f;
	}

	void UpdateBat(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position,
		float animation_elapsed)
	{
		switch (runtime.State)
		{
		case ActionState::BatCooldown:
		{
			enemy.Update(delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <= BAT_DASH_TRIGGER_RANGE * BAT_DASH_TRIGGER_RANGE &&
				distance_squared >= BAT_DASH_MINIMUM_RANGE * BAT_DASH_MINIMUM_RANGE)
			{
				BeginBatWindup(
					runtime, enemy, player_position, animation_elapsed);
			}
			break;
		}

		case ActionState::BatWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed = std::fmod(
				runtime.AnimationElapsed + delta_time * 2.0f, 60.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = TraceDashEnd(
				runtime.TelegraphStart,
				runtime.LockedDirection,
				runtime.DashDistance,
				enemy.GetCollisionRadius());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const float path_distance = std::sqrt(GetDistanceSquared(
					runtime.TelegraphStart, runtime.TelegraphEnd));
				runtime.State = ActionState::BatDash;
				runtime.Timer = path_distance / BAT_DASH_SPEED;
				if (g_BatDashAudioID >= 0)
				{
					PlayAudio(g_BatDashAudioID);
				}
			}
			break;

		case ActionState::BatDash:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
				runtime.LockedDirection.x * BAT_DASH_SPEED * movement_time,
				runtime.LockedDirection.y * BAT_DASH_SPEED * movement_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::BatRecovery;
				runtime.Timer = BAT_DASH_RECOVERY_DURATION;
			}
			break;
		}

		case ActionState::BatRecovery:
			enemy.Update(delta_time, player_position, 0.15f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::BatCooldown;
				runtime.Timer = GetRandomDuration(enemy_id, runtime, 2.4f, 3.6f);
			}
			break;

		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}

	void UpdateSkeletonRogue(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::SkeletonRogueCooldown:
		{
			UpdateSkeletonRogueMovement(
				enemy_id, runtime, enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <=
					SKELETON_ROGUE_THROW_RANGE * SKELETON_ROGUE_THROW_RANGE &&
				distance_squared >= SKELETON_ROGUE_THROW_MINIMUM_RANGE *
					SKELETON_ROGUE_THROW_MINIMUM_RANGE)
			{
				runtime.TelegraphStart = GetAttackOrigin(enemy);
				runtime.LockedDirection = GetDirection(
					runtime.TelegraphStart, player_position);
				runtime.State = ActionState::SkeletonRogueWindup;
				runtime.Timer = SKELETON_ROGUE_WINDUP_DURATION;
			}
			break;
		}

		case ActionState::SkeletonRogueWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const float muzzle_offset = enemy.GetCollisionRadius() + 12.0f;
				FireDaggerFan(
					GetAttackOrigin(enemy), runtime.LockedDirection, muzzle_offset);
			const DirectX::XMFLOAT2 away_direction{
				-runtime.LockedDirection.x,
				-runtime.LockedDirection.y,
			};
			const DirectX::XMFLOAT2 side_direction{
				-runtime.LockedDirection.y * runtime.StrafeDirection,
				runtime.LockedDirection.x * runtime.StrafeDirection,
			};
			const DirectX::XMFLOAT2 evade_direction{
				away_direction.x * 0.62f + side_direction.x * 0.78f,
				away_direction.y * 0.62f + side_direction.y * 0.78f,
			};
			runtime.LockedDirection = GetDirection(
				{ 0.0f, 0.0f }, evade_direction);
				runtime.State = ActionState::SkeletonRogueBackstep;
				runtime.Timer = SKELETON_ROGUE_EVADE_DURATION;
			}
			break;

		case ActionState::SkeletonRogueBackstep:
		{
			const DirectX::XMFLOAT2 evade_target{
				enemy.GetPosition().x + runtime.LockedDirection.x * 240.0f,
				enemy.GetPosition().y + runtime.LockedDirection.y * 240.0f,
			};
			enemy.Update(delta_time, evade_target, 2.0f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SkeletonRogueCooldown;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 2.6f, 3.8f);
			}
			break;
		}

		default:
			runtime.State = ActionState::SkeletonRogueCooldown;
			runtime.Timer = 1.0f;
			UpdateSkeletonRogueMovement(
				enemy_id, runtime, enemy, delta_time, player_position);
			break;
		}
	}

	void UpdateWarrior(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::WarriorChase:
		{
			const bool orc_warrior =
				runtime.Type == MonsterType::OrcWarrior;
			enemy.Update(
				delta_time,
				player_position,
				orc_warrior ? ORC_WARRIOR_CHASE_SPEED_SCALE : 1.18f);
			runtime.Timer -= delta_time;
			const float distance_squared = GetDistanceSquared(
				GetAttackOrigin(enemy), player_position);
			const float action_trigger_distance = orc_warrior ?
				ORC_WARRIOR_SIDESTEP_TRIGGER_DISTANCE :
				WARRIOR_ATTACK_TRIGGER_DISTANCE;
			if (runtime.Timer <= 0.0f &&
				distance_squared <= action_trigger_distance *
					action_trigger_distance)
			{
				if (orc_warrior)
				{
					BeginOrcWarriorSidestep(
						enemy_id, runtime, enemy);
				}
				else
				{
					runtime.LockedDirection = GetDirection(
						GetAttackOrigin(enemy), player_position);
					runtime.State = ActionState::WarriorWindup;
					runtime.Timer = WARRIOR_ATTACK_WINDUP_DURATION;
				}
			}
			break;
		}

		case ActionState::WarriorSidestep:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const DirectX::XMFLOAT2 sidestep_direction =
				GetOrcWarriorSidestepDirection(
					GetAttackOrigin(enemy), player_position, runtime.StrafeDirection);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
				sidestep_direction.x * ORC_WARRIOR_SIDESTEP_SPEED * movement_time,
				sidestep_direction.y * ORC_WARRIOR_SIDESTEP_SPEED * movement_time,
			});
			WarriorDashTrailRuntime& active_trail =
				runtime.WarriorDashTrails[
					static_cast<std::size_t>(runtime.MovementStep) %
					ORC_WARRIOR_DASH_TRAIL_COUNT];
			active_trail.End = enemy.GetPosition();
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				++runtime.MovementStep;
				if (runtime.MovementStep < ORC_WARRIOR_SIDESTEP_COUNT)
				{
					runtime.StrafeDirection *= -1.0f;
					runtime.Timer = ORC_WARRIOR_SIDESTEP_DURATION;
					BeginOrcWarriorDashTrail(
						runtime, enemy.GetPosition());
				}
				else
				{
					const float distance_squared = GetDistanceSquared(
						GetAttackOrigin(enemy), player_position);
					if (distance_squared <= WARRIOR_ATTACK_TRIGGER_DISTANCE *
						WARRIOR_ATTACK_TRIGGER_DISTANCE)
					{
						runtime.LockedDirection = GetDirection(
							GetAttackOrigin(enemy), player_position);
						runtime.State = ActionState::WarriorWindup;
						runtime.Timer = WARRIOR_ATTACK_WINDUP_DURATION;
					}
					else
					{
						runtime.State = ActionState::WarriorChase;
						runtime.Timer = 0.18f;
					}
				}
			}
			break;
		}

		case ActionState::WarriorWindup:
			enemy.Update(delta_time, player_position, 0.08f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = {
				runtime.TelegraphStart.x + runtime.LockedDirection.x *
					(WARRIOR_STRIKE_OFFSET + WARRIOR_STRIKE_RADIUS),
				runtime.TelegraphStart.y + runtime.LockedDirection.y *
					(WARRIOR_STRIKE_OFFSET + WARRIOR_STRIKE_RADIUS),
			};
			runtime.TelegraphWidth = WARRIOR_STRIKE_RADIUS * 1.45f;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 strike_position{
					runtime.TelegraphStart.x + runtime.LockedDirection.x *
						WARRIOR_STRIKE_OFFSET,
					runtime.TelegraphStart.y + runtime.LockedDirection.y *
						WARRIOR_STRIKE_OFFSET,
				};
				SpawnWarriorStrike(
					strike_position, runtime.LockedDirection, runtime.Type);
				runtime.State = ActionState::WarriorRecovery;
				runtime.Timer = WARRIOR_ATTACK_RECOVERY_DURATION;
			}
			break;

		case ActionState::WarriorRecovery:
			enemy.Update(delta_time, player_position, 0.18f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::WarriorChase;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 0.85f, 1.25f);
			}
			break;

		default:
			runtime.State = ActionState::WarriorChase;
			runtime.Timer = 0.5f;
			enemy.Update(
				delta_time,
				player_position,
				runtime.Type == MonsterType::OrcWarrior ?
					ORC_WARRIOR_CHASE_SPEED_SCALE : 1.18f);
			break;
		}
	}

	void UpdateOrc(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::OrcCooldown:
		{
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
			const float distance_squared = GetDistanceSquared(
				attack_origin, player_position);
			if (runtime.Timer <= 0.0f &&
				distance_squared <= ORC_AXE_THROW_RANGE * ORC_AXE_THROW_RANGE &&
				distance_squared >= ORC_AXE_THROW_MINIMUM_RANGE *
					ORC_AXE_THROW_MINIMUM_RANGE)
			{
				runtime.LockedDirection = GetDirection(
					attack_origin, player_position);
				runtime.TelegraphStart = attack_origin;
				runtime.TelegraphEnd = TraceDashEnd(
					attack_origin,
					runtime.LockedDirection,
					ORC_AXE_PROJECTILE_MAX_DISTANCE,
					ORC_AXE_PROJECTILE_RADIUS);
				runtime.TelegraphWidth = ORC_AXE_PROJECTILE_RADIUS * 1.25f;
				runtime.State = ActionState::OrcWindup;
				runtime.Timer = ORC_AXE_THROW_WINDUP_DURATION;
			}
			break;
		}

		case ActionState::OrcWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = TraceDashEnd(
				runtime.TelegraphStart,
				runtime.LockedDirection,
				ORC_AXE_PROJECTILE_MAX_DISTANCE,
				ORC_AXE_PROJECTILE_RADIUS);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
				SpawnAxeProjectile({
					attack_origin.x + runtime.LockedDirection.x * 36.0f,
					attack_origin.y + runtime.LockedDirection.y * 36.0f,
				}, runtime.LockedDirection, enemy_id);
				runtime.State = ActionState::OrcCooldown;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 2.1f, 3.0f);
			}
			break;

		default:
			runtime.State = ActionState::OrcCooldown;
			runtime.Timer = 1.0f;
			enemy.Update(delta_time, player_position, 0.8f);
			break;
		}
	}

	void UpdateRogue(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::RogueApproach:
		{
			const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
			const float distance_squared = GetDistanceSquared(
				attack_origin, player_position);
			enemy.Update(
				delta_time,
				player_position,
				distance_squared > ROGUE_STALK_DISTANCE * ROGUE_STALK_DISTANCE ?
					1.30f : 0.0f);
			const DirectX::XMFLOAT2 direction = GetDirection(
				GetAttackOrigin(enemy), player_position);
			enemy.ApplySeparation({
				-direction.y * ROGUE_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
				direction.x * ROGUE_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				PlayRogueSmoke(GetAttackOrigin(enemy));
				runtime.State = ActionState::RogueVanish;
				runtime.Timer = ROGUE_VANISH_DURATION;
				runtime.MovementStep = 0;
				BeginOrcWarriorDashTrail(runtime, enemy.GetPosition());
			}
			break;
		}

		case ActionState::RogueVanish:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
			const DirectX::XMFLOAT2 toward_player = GetDirection(
				attack_origin, player_position);
			const DirectX::XMFLOAT2 side_direction{
				-toward_player.y * runtime.StrafeDirection,
				toward_player.x * runtime.StrafeDirection,
			};
			const DirectX::XMFLOAT2 flank_position{
				player_position.x + side_direction.x * ROGUE_FLANK_DISTANCE,
				player_position.y + side_direction.y * ROGUE_FLANK_DISTANCE,
			};
			const float flank_x = flank_position.x - attack_origin.x;
			const float flank_y = flank_position.y - attack_origin.y;
			const float flank_distance = std::sqrt(
				flank_x * flank_x + flank_y * flank_y);
			if (flank_distance > 0.001f)
			{
				const float move_distance = std::min(
					ROGUE_STEALTH_MOVE_SPEED * delta_time, flank_distance);
				enemy.ApplySeparation({
					flank_x / flank_distance * move_distance,
					flank_y / flank_distance * move_distance,
				});
			}
			WarriorDashTrailRuntime& stealth_trail =
				runtime.WarriorDashTrails[0];
			stealth_trail.End = enemy.GetPosition();
			stealth_trail.TimeRemaining = ORC_WARRIOR_DASH_TRAIL_DURATION;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 new_origin = GetAttackOrigin(enemy);
				PlayRogueSmoke(new_origin);
				runtime.LockedDirection = GetDirection(
					new_origin, player_position);
				runtime.TelegraphStart = new_origin;
				runtime.TelegraphEnd = {
					new_origin.x + runtime.LockedDirection.x *
						(ROGUE_STRIKE_OFFSET + ROGUE_STRIKE_RADIUS),
					new_origin.y + runtime.LockedDirection.y *
						(ROGUE_STRIKE_OFFSET + ROGUE_STRIKE_RADIUS),
				};
				runtime.TelegraphWidth = ROGUE_STRIKE_RADIUS * 1.25f;
				runtime.State = ActionState::RogueBackstabWindup;
				runtime.Timer = ROGUE_BACKSTAB_WINDUP_DURATION;
			}
			break;
		}

		case ActionState::RogueBackstabWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = {
				runtime.TelegraphStart.x + runtime.LockedDirection.x *
					(ROGUE_STRIKE_OFFSET + ROGUE_STRIKE_RADIUS),
				runtime.TelegraphStart.y + runtime.LockedDirection.y *
					(ROGUE_STRIKE_OFFSET + ROGUE_STRIKE_RADIUS),
			};
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
				const DirectX::XMFLOAT2 strike_position{
					attack_origin.x + runtime.LockedDirection.x * ROGUE_STRIKE_OFFSET,
					attack_origin.y + runtime.LockedDirection.y * ROGUE_STRIKE_OFFSET,
				};
				SpawnRogueStrike(strike_position, runtime.LockedDirection, 0);
				runtime.State = ActionState::RogueRetreat;
				runtime.Timer = ROGUE_RETREAT_DURATION;
			}
			break;

		case ActionState::RogueBackstabStrike:
			runtime.State = ActionState::RogueRetreat;
			runtime.Timer = ROGUE_RETREAT_DURATION;
			break;

		case ActionState::RogueRetreat:
		{
			enemy.Update(delta_time, player_position, -1.60f);
			const DirectX::XMFLOAT2 direction = GetDirection(
				GetAttackOrigin(enemy), player_position);
			enemy.ApplySeparation({
				-direction.y * ROGUE_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
				direction.x * ROGUE_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::RogueApproach;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 2.2f, 3.2f);
				runtime.StrafeDirection *= -1.0f;
			}
			break;
		}

		default:
			runtime.State = ActionState::RogueApproach;
			runtime.Timer = 0.8f;
			enemy.Update(delta_time, player_position, 1.30f);
			break;
		}
	}

	void UpdateShaman(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::ShamanCooldown:
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f &&
				GetDistanceSquared(enemy.GetPosition(), player_position) <=
				SHAMAN_CAST_RANGE * SHAMAN_CAST_RANGE)
			{
				runtime.State = ActionState::ShamanCast;
				runtime.Timer = SHAMAN_CAST_DURATION;
				runtime.TargetPosition = player_position;
			}
			break;

		case ActionState::ShamanCast:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				SpawnGroundHazard(runtime.TargetPosition);
				runtime.State = ActionState::ShamanRecovery;
				runtime.Timer = SHAMAN_RECOVERY_DURATION;
			}
			break;

		case ActionState::ShamanRecovery:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::ShamanCooldown;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime, 3.8f, 5.2f);
			}
			break;

		default:
			runtime.State = ActionState::ShamanCooldown;
			runtime.Timer = 1.0f;
			UpdateRangedMovement(
				enemy_id, enemy, delta_time, player_position);
			break;
		}
	}

	void PlayCthulhuWarpEffect(const DirectX::XMFLOAT2& position)
	{
		cGameEffectManager::GetInstance().Play(
			GameEffectType::SmokePoof,
			position,
			1.45f,
			{ 0.28f, 0.04f, 0.72f, 0.94f });
	}

	void EmitCthulhuDashTrail(
		const DirectX::XMFLOAT2& start,
		const DirectX::XMFLOAT2& end)
	{
		if (g_WarningTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		const float dx = end.x - start.x;
		const float dy = end.y - start.y;
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance <= 0.5f)
		{
			return;
		}
		const float rotation = std::atan2(dy, dx);
		const DirectX::XMFLOAT2 center{
			(start.x + end.x) * 0.5f,
			(start.y + end.y) * 0.5f,
		};

		cTrailDesc ribbon{};
		ribbon.Position = center;
		ribbon.Width = distance + 150.0f;
		ribbon.Height = 132.0f;
		ribbon.StartScale = 1.0f;
		ribbon.EndScale = 0.72f;
		ribbon.Rotation = rotation;
		ribbon.LifeTime = 0.18f;
		ribbon.TextureID = g_WarningTextureID;
		ribbon.Color = { 0.08f, 0.72f, 0.60f, 0.22f };
		TrailSystem_Emit(ribbon);

		for (int streak_index = -1; streak_index <= 1; ++streak_index)
		{
			cTrailDesc streak = ribbon;
			streak.Position.y += static_cast<float>(streak_index) * 42.0f;
			streak.Width = distance + 105.0f +
				static_cast<float>(std::abs(streak_index)) * 34.0f;
			streak.Height = streak_index == 0 ? 18.0f : 8.0f;
			streak.EndScale = 0.18f;
			streak.LifeTime = streak_index == 0 ? 0.14f : 0.11f;
			streak.Color = streak_index == 0 ?
				DirectX::XMFLOAT4{ 0.30f, 1.0f, 0.76f, 0.62f } :
				DirectX::XMFLOAT4{ 0.45f, 0.82f, 1.0f, 0.42f };
			TrailSystem_Emit(streak);
		}
	}

	void BeginCthulhuDashWindup(
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		const DirectX::XMFLOAT2& player_position)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(
			ProceduralMap_GetRoomIndexAt(player_position));
		const bool from_left = (runtime.MovementStep & 1) == 0;
		DirectX::XMFLOAT2 dash_start = enemy.GetPosition();
		float dash_end_x = dash_start.x + (from_left ? 1000.0f : -1000.0f);
		if (room)
		{
			const float minimum_x = room->WorldMin.x + CTHULHU_ROOM_EDGE_PADDING;
			const float maximum_x = room->WorldMax.x - CTHULHU_ROOM_EDGE_PADDING;
			const float minimum_y = room->WorldMin.y + CTHULHU_ROOM_EDGE_PADDING;
			const float maximum_y = room->WorldMax.y - CTHULHU_ROOM_EDGE_PADDING;
			dash_start = {
				from_left ? minimum_x : maximum_x,
				std::clamp(player_position.y, minimum_y, maximum_y),
			};
			dash_end_x = from_left ? maximum_x : minimum_x;
		}

		const DirectX::XMFLOAT2 current_position = enemy.GetPosition();
		enemy.ApplySeparation({
			dash_start.x - current_position.x,
			dash_start.y - current_position.y,
		});
		runtime.LockedDirection = { from_left ? 1.0f : -1.0f, 0.0f };
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.DashDistance = std::abs(dash_end_x - runtime.TelegraphStart.x);
		runtime.TelegraphEnd = TraceDashEnd(
			runtime.TelegraphStart,
			runtime.LockedDirection,
			runtime.DashDistance,
			enemy.GetMapCollisionRadius());
		runtime.TelegraphWidth = CTHULHU_DASH_TELEGRAPH_WIDTH;
		runtime.State = ActionState::CthulhuDashWindup;
		runtime.Timer = CTHULHU_DASH_WINDUP_DURATION;
		runtime.AnimationElapsed = 0.0f;
		PlayCthulhuWarpEffect(runtime.TelegraphStart);
	}

	void UpdateCthulhu(
		int enemy_id,
		EnemyPatternRuntime& runtime,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::CthulhuRoam:
		{
			enemy.Update(delta_time, player_position, 0.32f);
			const DirectX::XMFLOAT2 direction = GetDirection(
				enemy.GetPosition(), player_position);
			enemy.ApplySeparation({
				-direction.y * CTHULHU_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
				direction.x * CTHULHU_STRAFE_SPEED *
					runtime.StrafeDirection * delta_time,
			});
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::CthulhuCast;
				runtime.Timer = CTHULHU_CAST_DURATION;
				runtime.AnimationElapsed = 0.0f;
			}
			break;
		}

		case ActionState::CthulhuCast:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				++runtime.MovementStep;
				runtime.State = ActionState::CthulhuVanish;
				runtime.Timer = CTHULHU_VANISH_DURATION;
				runtime.AnimationElapsed = 0.0f;
				PlayCthulhuWarpEffect(GetAttackOrigin(enemy));
			}
			break;

		case ActionState::CthulhuVanish:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				BeginCthulhuDashWindup(runtime, enemy, player_position);
			}
			break;

		case ActionState::CthulhuDashWindup:
			enemy.Update(delta_time, {
				enemy.GetPosition().x + runtime.LockedDirection.x * 100.0f,
				enemy.GetPosition().y,
			}, 0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const float path_distance = std::sqrt(GetDistanceSquared(
					runtime.TelegraphStart, runtime.TelegraphEnd));
				runtime.State = ActionState::CthulhuDash;
				runtime.Timer = path_distance / CTHULHU_DASH_SPEED;
				runtime.AnimationElapsed = 0.0f;
				if (g_BatDashAudioID >= 0)
				{
					PlayAudio(g_BatDashAudioID);
				}
				cGameEffectManager::GetInstance().Play(
					GameEffectType::VoidImplosion,
					enemy.GetPosition(),
					1.18f,
					{ 0.18f, 1.0f, 0.68f, 0.96f });
			}
			break;

		case ActionState::CthulhuDash:
		{
			enemy.Update(delta_time, {
				enemy.GetPosition().x + runtime.LockedDirection.x * 100.0f,
				enemy.GetPosition().y,
			}, 0.0f);
			const DirectX::XMFLOAT2 previous_position = enemy.GetPosition();
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
				runtime.LockedDirection.x * CTHULHU_DASH_SPEED * movement_time,
				0.0f,
			});
			runtime.AnimationElapsed += delta_time;
			EmitCthulhuDashTrail(previous_position, enemy.GetPosition());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				cGameEffectManager::GetInstance().Play(
					GameEffectType::VoidImplosion,
					enemy.GetPosition(),
					1.05f,
					{ 0.18f, 0.86f, 1.0f, 0.92f });
				runtime.State = ActionState::CthulhuRecovery;
				runtime.Timer = CTHULHU_DASH_RECOVERY_DURATION;
				runtime.AnimationElapsed = 0.0f;
			}
			break;
		}

		case ActionState::CthulhuRecovery:
			enemy.Update(delta_time, player_position, 0.08f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::CthulhuRoam;
				runtime.Timer = GetRandomDuration(
					enemy_id, runtime,
					CTHULHU_ROAM_MIN_DURATION,
					CTHULHU_ROAM_MAX_DURATION);
				runtime.StrafeDirection *= -1.0f;
				runtime.AnimationElapsed = 0.0f;
			}
			break;

		default:
			runtime.State = ActionState::CthulhuRoam;
			runtime.Timer = CTHULHU_ROAM_MIN_DURATION;
			runtime.AnimationElapsed = 0.0f;
			enemy.Update(delta_time, player_position, 0.25f);
			break;
		}
	}
}

namespace EnemyAttackPattern
{
void Initialize(
	int bone_texture_id,
	int dagger_texture_id,
	int axe_texture_id,
	int mage_projectile_texture_id,
	int warning_texture_id,
	int ground_effect_texture_id,
	int bone_throw_audio_id,
	const std::array<int, 3>& mage_fire_audio_ids,
	int warrior_slash_audio_id,
	int bat_dash_audio_id,
	int shaman_cast_audio_id)
{
	g_BoneTextureID = bone_texture_id;
	g_DaggerTextureID = dagger_texture_id;
	g_AxeTextureID = axe_texture_id;
	g_MageProjectileTextureID = mage_projectile_texture_id;
	g_WarningTextureID = warning_texture_id;
	g_GroundEffectTextureID = ground_effect_texture_id;
	g_BoneThrowAudioID = bone_throw_audio_id;
	g_MageFireAudioIDs = mage_fire_audio_ids;
	g_WarriorSlashAudioID = warrior_slash_audio_id;
	g_BatDashAudioID = bat_dash_audio_id;
	g_ShamanCastAudioID = shaman_cast_audio_id;
}

void Finalize()
{
	Reset(0);
	g_BoneTextureID = TEXTURE_INVALID_ID;
	g_DaggerTextureID = TEXTURE_INVALID_ID;
	g_AxeTextureID = TEXTURE_INVALID_ID;
	g_MageProjectileTextureID = TEXTURE_INVALID_ID;
	g_WarningTextureID = TEXTURE_INVALID_ID;
	g_GroundEffectTextureID = TEXTURE_INVALID_ID;
	g_BoneThrowAudioID = -1;
	g_MageFireAudioIDs.fill(-1);
	g_WarriorSlashAudioID = -1;
	g_BatDashAudioID = -1;
	g_ShamanCastAudioID = -1;
}

void Reset(std::uint32_t seed)
{
	g_RandomSeed = 0x51A1D45Eu ^ seed;
	for (EnemyPatternRuntime& runtime : g_EnemyPatterns)
	{
		runtime = EnemyPatternRuntime{};
	}
	for (BoneProjectile& projectile : g_BoneProjectiles)
	{
		projectile = BoneProjectile{};
	}
	for (DaggerProjectile& projectile : g_DaggerProjectiles)
	{
		projectile = DaggerProjectile{};
	}
	for (MageProjectile& projectile : g_MageProjectiles)
	{
		projectile = MageProjectile{};
	}
	for (WarriorStrike& strike : g_WarriorStrikes)
	{
		strike = WarriorStrike{};
	}
	for (GroundHazard& hazard : g_GroundHazards)
	{
		hazard = GroundHazard{};
	}
}

void OnSpawn(int enemy_id, MonsterType type)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return;
	}

	EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	runtime = EnemyPatternRuntime{};
	runtime.Type = type;
	runtime.IsActive = true;
	if (type == MonsterType::Slime)
	{
		runtime.State = ActionState::SlimeCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 1.7f, 3.0f);
	}
	else if (IsBoneThrower(type))
	{
		runtime.State = ActionState::SkeletonCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 1.2f, 2.5f);
	}
	else if (type == MonsterType::SkeletonMage)
	{
		runtime.State = ActionState::SkeletonMageCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 1.4f, 2.4f);
	}
	else if (type == MonsterType::Bat)
	{
		runtime.State = ActionState::BatCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 0.9f, 1.8f);
	}
	else if (type == MonsterType::SkeletonRogue)
	{
		runtime.State = ActionState::SkeletonRogueCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 1.0f, 2.0f);
		runtime.StrafeDirection = (Hash32(
			static_cast<std::uint32_t>(enemy_id + 1) ^ g_RandomSeed) & 1u) == 0u ?
			-1.0f : 1.0f;
		runtime.MovementTimer = GetRandomDuration(
			enemy_id, runtime, 1.15f, 2.25f);
	}
	else if (IsWarriorType(type))
	{
		runtime.State = ActionState::WarriorChase;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 0.35f, 0.8f);
	}
	else if (type == MonsterType::Orc)
	{
		runtime.State = ActionState::OrcCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 0.8f, 1.5f);
	}
	else if (IsOrcRogue(type))
	{
		runtime.State = ActionState::RogueApproach;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 0.8f, 1.4f);
		runtime.StrafeDirection = (Hash32(
			static_cast<std::uint32_t>(enemy_id + 1) ^ g_RandomSeed) & 1u) == 0u ?
			-1.0f : 1.0f;
	}
	else if (type == MonsterType::OrcShaman)
	{
		runtime.State = ActionState::ShamanCooldown;
		runtime.Timer = GetRandomDuration(enemy_id, runtime, 1.5f, 2.8f);
	}
	else if (type == MonsterType::BossCthulhu)
	{
		runtime.State = ActionState::CthulhuRoam;
		runtime.Timer = GetRandomDuration(
			enemy_id, runtime,
			CTHULHU_ROAM_MIN_DURATION,
			CTHULHU_ROAM_MAX_DURATION);
		runtime.StrafeDirection = (Hash32(
			static_cast<std::uint32_t>(enemy_id + 1) ^ g_RandomSeed) & 1u) == 0u ?
			-1.0f : 1.0f;
	}
}

void OnDeactivate(int enemy_id)
{
	if (IsValidEnemyID(enemy_id))
	{
		g_EnemyPatterns[enemy_id] = EnemyPatternRuntime{};
	}
}

void UpdateEnemy(
	int enemy_id,
	MonsterType type,
	cEnemy& enemy,
	float delta_time,
	const DirectX::XMFLOAT2& player_position,
	float visual_bottom_offset,
	float animation_elapsed)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	if (!IsValidEnemyID(enemy_id))
	{
		enemy.Update(safe_delta_time, player_position);
		return;
	}

	EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	if (!enemy.IsAlive())
	{
		enemy.Update(safe_delta_time, player_position);
		runtime = EnemyPatternRuntime{};
		return;
	}
	if (!runtime.IsActive || runtime.Type != type)
	{
		OnSpawn(enemy_id, type);
	}
	for (WarriorDashTrailRuntime& trail : runtime.WarriorDashTrails)
	{
		trail.TimeRemaining = std::max(
			0.0f, trail.TimeRemaining - safe_delta_time);
	}

	if (type == MonsterType::Slime)
	{
		UpdateSlime(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (IsBoneThrower(type))
	{
		UpdateSkeleton(
			enemy_id,
			runtime,
			enemy,
			safe_delta_time,
			player_position);
	}
	else if (type == MonsterType::SkeletonMage)
	{
		UpdateSkeletonMage(
			enemy_id,
			runtime,
			enemy,
			safe_delta_time,
			player_position,
			visual_bottom_offset);
	}
	else if (type == MonsterType::Bat)
	{
		UpdateBat(
			enemy_id,
			runtime,
			enemy,
			safe_delta_time,
			player_position,
			animation_elapsed);
	}
	else if (type == MonsterType::SkeletonRogue)
	{
		UpdateSkeletonRogue(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (type == MonsterType::Orc)
	{
		UpdateOrc(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (IsOrcRogue(type))
	{
		UpdateRogue(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (type == MonsterType::OrcShaman)
	{
		UpdateShaman(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (IsWarriorType(type))
	{
		UpdateWarrior(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else if (type == MonsterType::BossCthulhu)
	{
		UpdateCthulhu(
			enemy_id, runtime, enemy, safe_delta_time, player_position);
	}
	else
	{
		enemy.Update(safe_delta_time, player_position);
	}
}

float GetAnimationElapsed(int enemy_id, float default_elapsed)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return default_elapsed;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	if (runtime.IsActive && runtime.Type == MonsterType::Bat &&
		(runtime.State == ActionState::BatWindup ||
			runtime.State == ActionState::BatDash))
	{
		return runtime.AnimationElapsed;
	}
	return default_elapsed;
}

bool GetAnimationFrameRegion(
	int enemy_id,
	int& out_frame_x,
	int& out_frame_y)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return false;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	if (!runtime.IsActive || runtime.Type != MonsterType::BossCthulhu)
	{
		return false;
	}

	int row = CTHULHU_IDLE_ROW;
	int frame_count = CTHULHU_IDLE_FRAME_COUNT;
	bool loop = true;
	switch (runtime.State)
	{
	case ActionState::CthulhuRoam:
		row = CTHULHU_WALK_ROW;
		frame_count = CTHULHU_WALK_FRAME_COUNT;
		break;
	case ActionState::CthulhuCast:
		if ((runtime.MovementStep & 1) == 0)
		{
			row = CTHULHU_ATTACK_ONE_ROW;
			frame_count = CTHULHU_ATTACK_ONE_FRAME_COUNT;
		}
		else
		{
			row = CTHULHU_ATTACK_TWO_ROW;
			frame_count = CTHULHU_ATTACK_TWO_FRAME_COUNT;
		}
		loop = false;
		break;
	case ActionState::CthulhuVanish:
	case ActionState::CthulhuDashWindup:
	case ActionState::CthulhuDash:
		row = CTHULHU_FLY_ROW;
		frame_count = CTHULHU_FLY_FRAME_COUNT;
		break;
	case ActionState::CthulhuRecovery:
	default:
		break;
	}

	const int raw_frame = static_cast<int>(
		runtime.AnimationElapsed / CTHULHU_ANIMATION_FRAME_DURATION);
	const int frame = loop ? raw_frame % frame_count :
		std::min(raw_frame, frame_count - 1);
	out_frame_x = frame * CTHULHU_FRAME_WIDTH;
	out_frame_y = row * CTHULHU_FRAME_HEIGHT;
	return true;
}

float GetVisualAlpha(int enemy_id)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return 1.0f;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	if (!runtime.IsActive)
	{
		return 1.0f;
	}
	if (runtime.Type == MonsterType::OrcRogue &&
		runtime.State == ActionState::RogueVanish)
	{
		return 0.42f;
	}
	if (runtime.Type == MonsterType::BossCthulhu)
	{
		if (runtime.State == ActionState::CthulhuVanish)
		{
			return std::clamp(
				runtime.Timer / CTHULHU_VANISH_DURATION, 0.0f, 1.0f);
		}
		if (runtime.State == ActionState::CthulhuDashWindup)
		{
			return 1.0f - std::clamp(
				runtime.Timer / CTHULHU_DASH_WINDUP_DURATION, 0.0f, 1.0f);
		}
	}
	return 1.0f;
}

bool IsTargetable(int enemy_id)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return false;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	return !runtime.IsActive || runtime.Type != MonsterType::BossCthulhu ||
		runtime.State != ActionState::CthulhuVanish;
}

bool IsBossAttackWindow(int enemy_id)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return true;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	return !runtime.IsActive || runtime.Type != MonsterType::BossCthulhu ||
		runtime.State == ActionState::CthulhuCast;
}

int GetBossAttackVariant(int enemy_id)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return 0;
	}
	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	return runtime.IsActive && runtime.Type == MonsterType::BossCthulhu ?
		runtime.MovementStep % 4 : 0;
}

int GetDashAfterimagePaths(
	int enemy_id,
	DashAfterimagePath* out_paths,
	int capacity)
{
	if (!IsValidEnemyID(enemy_id) || !out_paths || capacity <= 0)
	{
		return 0;
	}

	const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
	if (!runtime.IsActive ||
		(runtime.Type != MonsterType::OrcWarrior &&
			runtime.Type != MonsterType::OrcRogue))
	{
		return 0;
	}

	int path_count = 0;
	for (const WarriorDashTrailRuntime& trail : runtime.WarriorDashTrails)
	{
		const float path_x = trail.End.x - trail.Start.x;
		const float path_y = trail.End.y - trail.Start.y;
		if (trail.TimeRemaining <= 0.0f || path_count >= capacity ||
			path_x * path_x + path_y * path_y <= 1.0f)
		{
			continue;
		}
		out_paths[path_count++] = {
			trail.Start,
			trail.End,
			std::clamp(
				trail.TimeRemaining / ORC_WARRIOR_DASH_TRAIL_DURATION,
				0.0f,
				1.0f),
		};
	}
	return path_count;
}

void UpdateProjectiles(float delta_time)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	for (BoneProjectile& projectile : g_BoneProjectiles)
	{
		if (!projectile.IsActive)
		{
			continue;
		}

		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position{
			previous_position.x + projectile.Velocity.x * safe_delta_time,
			previous_position.y + projectile.Velocity.y * safe_delta_time,
		};
		const float step_x = next_position.x - previous_position.x;
		const float step_y = next_position.y - previous_position.y;
		const float step_distance = std::sqrt(step_x * step_x + step_y * step_y);
		if (!ProceduralMap_IsSegmentWalkable(
			previous_position, next_position, BONE_PROJECTILE_RADIUS) ||
			projectile.Travelled + step_distance >= BONE_PROJECTILE_MAX_DISTANCE)
		{
			projectile.IsActive = false;
			continue;
		}

		projectile.Position = next_position;
		projectile.Travelled += step_distance;
		projectile.Rotation += projectile.AngularVelocity * safe_delta_time;
	}
	for (DaggerProjectile& projectile : g_DaggerProjectiles)
	{
		if (!projectile.IsActive)
		{
			continue;
		}
		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position{
			previous_position.x + projectile.Velocity.x * safe_delta_time,
			previous_position.y + projectile.Velocity.y * safe_delta_time,
		};
		const float step_x = next_position.x - previous_position.x;
		const float step_y = next_position.y - previous_position.y;
		const float step_distance = std::sqrt(step_x * step_x + step_y * step_y);
		if (!ProceduralMap_IsSegmentWalkable(
			previous_position, next_position, projectile.Radius) ||
			projectile.Travelled + step_distance >= projectile.MaxDistance)
		{
			projectile.IsActive = false;
			continue;
		}
		projectile.Position = next_position;
		projectile.Travelled += step_distance;
		projectile.Rotation += projectile.AngularVelocity * safe_delta_time;
	}
	for (MageProjectile& projectile : g_MageProjectiles)
	{
		if (!projectile.IsActive)
		{
			continue;
		}
		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position{
			previous_position.x + projectile.Velocity.x * safe_delta_time,
			previous_position.y + projectile.Velocity.y * safe_delta_time,
		};
		const float step_x = next_position.x - previous_position.x;
		const float step_y = next_position.y - previous_position.y;
		const float step_distance = std::sqrt(step_x * step_x + step_y * step_y);
		if (!ProceduralMap_IsSegmentWalkable(
			previous_position, next_position, MAGE_PROJECTILE_RADIUS) ||
			projectile.Travelled + step_distance >= MAGE_PROJECTILE_MAX_DISTANCE)
		{
			projectile.IsActive = false;
			continue;
		}
		projectile.Position = next_position;
		projectile.Travelled += step_distance;
		projectile.AnimationElapsed += safe_delta_time;
	}
	for (WarriorStrike& strike : g_WarriorStrikes)
	{
		if (!strike.IsActive)
		{
			continue;
		}
		strike.Elapsed += safe_delta_time;
		if (strike.Elapsed >= strike.Lifetime)
		{
			strike.IsActive = false;
		}
	}
	for (GroundHazard& hazard : g_GroundHazards)
	{
		if (!hazard.IsActive)
		{
			continue;
		}
		hazard.Elapsed += safe_delta_time;
		if (hazard.Elapsed >= hazard.Lifetime)
		{
			hazard.IsActive = false;
		}
	}
}

void DrawTelegraphs()
{
	static std::vector<SpriteInstance> line_instances;
	line_instances.clear();
	if (line_instances.capacity() < ENEMY_CAPACITY)
	{
		line_instances.reserve(ENEMY_CAPACITY);
	}
	if (g_WarningTextureID != TEXTURE_INVALID_ID)
	{
		for (const EnemyPatternRuntime& runtime : g_EnemyPatterns)
		{
			const bool slime_telegraph =
				runtime.State == ActionState::SlimeTelegraph;
			const bool bat_telegraph = runtime.State == ActionState::BatWindup;
			const bool orc_telegraph = runtime.State == ActionState::OrcWindup;
			const bool skeleton_rogue_telegraph =
				runtime.State == ActionState::SkeletonRogueWindup;
			const bool orc_rogue_telegraph =
				runtime.State == ActionState::RogueBackstabWindup;
			const bool mage_telegraph =
				runtime.State == ActionState::SkeletonMageWindup;
			const bool warrior_telegraph =
				runtime.State == ActionState::WarriorWindup;
			const bool cthulhu_telegraph =
				runtime.State == ActionState::CthulhuDashWindup;
			if (!runtime.IsActive ||
				(!slime_telegraph && !bat_telegraph && !orc_telegraph &&
					!skeleton_rogue_telegraph && !orc_rogue_telegraph &&
					!mage_telegraph && !warrior_telegraph &&
					!cthulhu_telegraph))
			{
				continue;
			}
			if (skeleton_rogue_telegraph)
			{
				const float progress = 1.0f - std::clamp(
					runtime.Timer / SKELETON_ROGUE_WINDUP_DURATION,
					0.0f,
					1.0f);
				for (int dagger_index = -1; dagger_index <= 1; ++dagger_index)
				{
					const DirectX::XMFLOAT2 direction = RotateDirection(
						runtime.LockedDirection,
						static_cast<float>(dagger_index) *
							SKELETON_ROGUE_DAGGER_ANGLE_STEP);
					const DirectX::XMFLOAT2 end = TraceDashEnd(
						runtime.TelegraphStart,
						direction,
						DAGGER_PROJECTILE_MAX_DISTANCE,
						DAGGER_PROJECTILE_RADIUS);
					const float dx = end.x - runtime.TelegraphStart.x;
					const float dy = end.y - runtime.TelegraphStart.y;
					const float length = std::sqrt(dx * dx + dy * dy);
					if (length <= 0.001f)
					{
						continue;
					}
					line_instances.push_back({
						{
							(runtime.TelegraphStart.x + end.x) * 0.5f,
							(runtime.TelegraphStart.y + end.y) * 0.5f,
						},
						{ length, 11.0f },
						std::atan2(dy, dx),
						{ 1.0f, 0.04f, 0.01f, 0.12f + progress * 0.18f },
					});
				}
				continue;
			}

			const float dx = runtime.TelegraphEnd.x - runtime.TelegraphStart.x;
			const float dy = runtime.TelegraphEnd.y - runtime.TelegraphStart.y;
			const float length_squared = dx * dx + dy * dy;
			if (length_squared <= 0.0001f)
			{
				continue;
			}
			const float length = std::sqrt(length_squared);
			const float telegraph_duration = slime_telegraph ?
				SLIME_DASH_TELEGRAPH_DURATION : (mage_telegraph ?
					SKELETON_MAGE_CAST_DURATION : (warrior_telegraph ?
						WARRIOR_ATTACK_WINDUP_DURATION : (orc_rogue_telegraph ?
							ROGUE_BACKSTAB_WINDUP_DURATION : (orc_telegraph ?
								ORC_AXE_THROW_WINDUP_DURATION : (cthulhu_telegraph ?
									CTHULHU_DASH_WINDUP_DURATION : BAT_DASH_WINDUP_DURATION)))));
			const float progress = 1.0f - std::clamp(
				runtime.Timer / telegraph_duration, 0.0f, 1.0f);
			const float pulse = 0.68f + 0.32f * std::abs(
				std::sin(progress * DirectX::XM_PI * 5.0f));
			const float alpha = cthulhu_telegraph ?
				0.16f + progress * 0.30f :
				(warrior_telegraph || orc_rogue_telegraph) ?
				0.10f + progress * 0.20f : (mage_telegraph ?
				0.10f + progress * 0.22f : (slime_telegraph ?
					0.16f + progress * 0.18f :
					0.12f + progress * 0.14f));
			line_instances.push_back({
				{
					(runtime.TelegraphStart.x + runtime.TelegraphEnd.x) * 0.5f,
					(runtime.TelegraphStart.y + runtime.TelegraphEnd.y) * 0.5f,
				},
				{ length, runtime.TelegraphWidth },
				std::atan2(dy, dx),
				{ 1.0f, 0.06f, 0.02f, alpha * pulse },
			});
		}
	}
	if (!line_instances.empty())
	{
		SpriteInstanced_DrawUnlit(
			g_WarningTextureID,
			line_instances.data(),
			static_cast<int>(line_instances.size()));
	}

	if (g_GroundEffectTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	static std::vector<SpriteInstance> ground_instances;
	static std::vector<SpriteInstance> shaman_ground_instances;
	ground_instances.clear();
	shaman_ground_instances.clear();
	if (ground_instances.capacity() < ENEMY_CAPACITY)
	{
		ground_instances.reserve(ENEMY_CAPACITY);
		shaman_ground_instances.reserve(ENEMY_CAPACITY);
	}
	for (const EnemyPatternRuntime& runtime : g_EnemyPatterns)
	{
		const bool shaman_cast = runtime.State == ActionState::ShamanCast;
		const bool mage_cast = runtime.State == ActionState::SkeletonMageWindup;
		if (!runtime.IsActive || (!shaman_cast && !mage_cast))
		{
			continue;
		}
		const float cast_duration = mage_cast ?
			SKELETON_MAGE_CAST_DURATION : SHAMAN_CAST_DURATION;
		const float progress = 1.0f - std::clamp(
			runtime.Timer / cast_duration, 0.0f, 1.0f);
		const float pulse = 0.86f + 0.14f * std::abs(
			std::sin(progress * DirectX::XM_PI * 7.0f));
		const float base_size = mage_cast ? 112.0f :
			SHAMAN_HAZARD_RADIUS * 2.0f;
		const float size = base_size * (0.86f + progress * 0.14f);
		const SpriteInstance ground_instance{
			runtime.TargetPosition,
			{ size, size },
			progress * 1.8f,
			{ 1.0f, 0.04f, 0.01f, (0.42f + progress * 0.30f) * pulse },
		};
		ground_instances.push_back(ground_instance);
		if (shaman_cast)
		{
			shaman_ground_instances.push_back(ground_instance);
		}
	}
	if (!ground_instances.empty())
	{
		if (!shaman_ground_instances.empty())
		{
			SpriteInstanced_DrawOutlinedUnlit(
				g_GroundEffectTextureID,
				shaman_ground_instances.data(),
				static_cast<int>(shaman_ground_instances.size()),
				ENEMY_PROJECTILE_OUTLINE_COLOR,
				ENEMY_PROJECTILE_OUTLINE_THICKNESS);
		}
		SpriteInstanced_DrawAdditiveUnlit(
			g_GroundEffectTextureID,
			ground_instances.data(),
			static_cast<int>(ground_instances.size()));
	}
}

void DrawProjectiles()
{
	static std::vector<SpriteInstance> bone_instances;
	bone_instances.clear();
	if (bone_instances.capacity() < BONE_PROJECTILE_CAPACITY)
	{
		bone_instances.reserve(BONE_PROJECTILE_CAPACITY);
	}
	if (g_BoneTextureID != TEXTURE_INVALID_ID)
	{
		for (const BoneProjectile& projectile : g_BoneProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			bone_instances.push_back({
				projectile.Position,
				{ BONE_PROJECTILE_WIDTH, BONE_PROJECTILE_HEIGHT },
				projectile.Rotation,
				{ 1.0f, 0.96f, 0.82f, 1.0f },
				{ 0.0f, 0.0f },
				{ 1.0f / 3.0f, 1.0f },
			});
		}
	}
	if (!bone_instances.empty())
	{
		SpriteInstanced_DrawOutlinedUnlit(
			g_BoneTextureID,
			bone_instances.data(),
			static_cast<int>(bone_instances.size()),
			ENEMY_PROJECTILE_OUTLINE_COLOR,
			ENEMY_PROJECTILE_OUTLINE_THICKNESS);
	}

	static std::vector<SpriteInstance> dagger_instances;
	static std::vector<SpriteInstance> axe_instances;
	dagger_instances.clear();
	axe_instances.clear();
	if (dagger_instances.capacity() < DAGGER_PROJECTILE_CAPACITY)
	{
		dagger_instances.reserve(DAGGER_PROJECTILE_CAPACITY);
		axe_instances.reserve(DAGGER_PROJECTILE_CAPACITY);
	}
	for (const DaggerProjectile& projectile : g_DaggerProjectiles)
	{
		if (!projectile.IsActive)
		{
			continue;
		}
		std::vector<SpriteInstance>& instances = projectile.IsAxe ?
			axe_instances : dagger_instances;
		instances.push_back({
			projectile.Position,
			{ projectile.Size, projectile.Size },
			projectile.Rotation,
			projectile.IsAxe ?
				DirectX::XMFLOAT4{ 1.0f, 0.82f, 0.62f, 1.0f } :
				DirectX::XMFLOAT4{ 1.0f, 0.90f, 0.78f, 1.0f },
		});
	}
	if (!dagger_instances.empty() && g_DaggerTextureID != TEXTURE_INVALID_ID)
	{
		SpriteInstanced_DrawOutlinedUnlit(
			g_DaggerTextureID,
			dagger_instances.data(),
			static_cast<int>(dagger_instances.size()),
			ENEMY_PROJECTILE_OUTLINE_COLOR,
			ENEMY_PROJECTILE_OUTLINE_THICKNESS);
	}
	if (!axe_instances.empty() && g_AxeTextureID != TEXTURE_INVALID_ID)
	{
		SpriteInstanced_DrawOutlinedUnlit(
			g_AxeTextureID,
			axe_instances.data(),
			static_cast<int>(axe_instances.size()),
			ENEMY_PROJECTILE_OUTLINE_COLOR,
			ENEMY_PROJECTILE_OUTLINE_THICKNESS);
	}

	static std::vector<SpriteInstance> mage_projectile_instances;
	mage_projectile_instances.clear();
	if (mage_projectile_instances.capacity() < MAGE_PROJECTILE_CAPACITY)
	{
		mage_projectile_instances.reserve(MAGE_PROJECTILE_CAPACITY);
	}
	if (g_MageProjectileTextureID != TEXTURE_INVALID_ID)
	{
		for (const MageProjectile& projectile : g_MageProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			const int frame = static_cast<int>(
				projectile.AnimationElapsed / MAGE_PROJECTILE_FRAME_TIME) %
				MAGE_PROJECTILE_FRAME_COUNT;
			const int frame_column = frame % MAGE_PROJECTILE_FRAME_COLUMNS;
			const int frame_row = frame / MAGE_PROJECTILE_FRAME_COLUMNS;
			mage_projectile_instances.push_back({
				projectile.Position,
				{ MAGE_PROJECTILE_SIZE, MAGE_PROJECTILE_SIZE },
				projectile.Rotation,
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				{
					static_cast<float>(frame_column) /
						static_cast<float>(MAGE_PROJECTILE_FRAME_COLUMNS),
					static_cast<float>(frame_row) / 2.0f,
				},
				{ 0.5f, 0.5f },
			});
		}
	}
	if (!mage_projectile_instances.empty())
	{
		SpriteInstanced_DrawOutlinedUnlit(
			g_MageProjectileTextureID,
			mage_projectile_instances.data(),
			static_cast<int>(mage_projectile_instances.size()),
			ENEMY_PROJECTILE_OUTLINE_COLOR,
			ENEMY_PROJECTILE_OUTLINE_THICKNESS);
	}

	if (g_GroundEffectTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	static std::vector<SpriteInstance> hazard_instances;
	hazard_instances.clear();
	if (hazard_instances.capacity() < GROUND_HAZARD_CAPACITY)
	{
		hazard_instances.reserve(GROUND_HAZARD_CAPACITY);
	}
	for (const GroundHazard& hazard : g_GroundHazards)
	{
		if (!hazard.IsActive)
		{
			continue;
		}
		const float progress = std::clamp(
			hazard.Elapsed / hazard.Lifetime, 0.0f, 1.0f);
		const float size = hazard.Radius * 2.0f * (1.0f + progress * 0.22f);
		hazard_instances.push_back({
			hazard.Position,
			{ size, size },
			progress * 2.6f,
			{ 1.0f, 0.18f, 0.01f, (1.0f - progress) * 0.92f },
		});
	}
	if (!hazard_instances.empty())
	{
		SpriteInstanced_DrawAdditiveUnlit(
			g_GroundEffectTextureID,
			hazard_instances.data(),
			static_cast<int>(hazard_instances.size()));
	}
}

void RegisterProjectileColliders(int owner_id_offset)
{
	for (int projectile_id = 0;
		projectile_id < BONE_PROJECTILE_CAPACITY;
		++projectile_id)
	{
		const BoneProjectile& projectile = g_BoneProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			owner_id_offset + projectile_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			projectile.Position,
			BONE_PROJECTILE_RADIUS);
	}
	for (int projectile_id = 0;
		projectile_id < DAGGER_PROJECTILE_CAPACITY;
		++projectile_id)
	{
		const DaggerProjectile& projectile = g_DaggerProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			owner_id_offset + BONE_PROJECTILE_CAPACITY + projectile_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			projectile.Position,
			projectile.Radius);
	}
	for (int projectile_id = 0;
		projectile_id < MAGE_PROJECTILE_CAPACITY;
		++projectile_id)
	{
		const MageProjectile& projectile = g_MageProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			owner_id_offset + BONE_PROJECTILE_CAPACITY +
				DAGGER_PROJECTILE_CAPACITY + projectile_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			projectile.Position,
			MAGE_PROJECTILE_RADIUS);
	}
	for (int strike_id = 0; strike_id < WARRIOR_STRIKE_CAPACITY; ++strike_id)
	{
		const WarriorStrike& strike = g_WarriorStrikes[strike_id];
		if (!strike.IsActive)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			owner_id_offset + BONE_PROJECTILE_CAPACITY +
				DAGGER_PROJECTILE_CAPACITY +
				MAGE_PROJECTILE_CAPACITY + strike_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			strike.Position,
			strike.Radius);
	}
	for (int hazard_id = 0; hazard_id < GROUND_HAZARD_CAPACITY; ++hazard_id)
	{
		const GroundHazard& hazard = g_GroundHazards[hazard_id];
		if (!hazard.IsActive)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			owner_id_offset + BONE_PROJECTILE_CAPACITY +
				DAGGER_PROJECTILE_CAPACITY +
				MAGE_PROJECTILE_CAPACITY +
				WARRIOR_STRIKE_CAPACITY + hazard_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			hazard.Position,
			hazard.Radius);
	}
}

bool ConsumeProjectile(int projectile_id, float& out_damage)
{
	if (projectile_id < 0)
	{
		return false;
	}
	if (projectile_id < BONE_PROJECTILE_CAPACITY)
	{
		BoneProjectile& projectile = g_BoneProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			return false;
		}
		out_damage = BONE_PROJECTILE_DAMAGE;
		projectile.IsActive = false;
		return true;
	}
	projectile_id -= BONE_PROJECTILE_CAPACITY;
	if (projectile_id < DAGGER_PROJECTILE_CAPACITY)
	{
		DaggerProjectile& projectile = g_DaggerProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			return false;
		}
		out_damage = projectile.Damage;
		projectile.IsActive = false;
		return true;
	}
	projectile_id -= DAGGER_PROJECTILE_CAPACITY;
	if (projectile_id < MAGE_PROJECTILE_CAPACITY)
	{
		MageProjectile& projectile = g_MageProjectiles[projectile_id];
		if (!projectile.IsActive)
		{
			return false;
		}
		out_damage = MAGE_PROJECTILE_DAMAGE;
		projectile.IsActive = false;
		return true;
	}
	projectile_id -= MAGE_PROJECTILE_CAPACITY;
	if (projectile_id < WARRIOR_STRIKE_CAPACITY)
	{
		WarriorStrike& strike = g_WarriorStrikes[projectile_id];
		if (!strike.IsActive)
		{
			return false;
		}
		out_damage = strike.Damage;
		strike.IsActive = false;
		return true;
	}
	projectile_id -= WARRIOR_STRIKE_CAPACITY;
	if (projectile_id < GROUND_HAZARD_CAPACITY)
	{
		GroundHazard& hazard = g_GroundHazards[projectile_id];
		if (!hazard.IsActive)
		{
			return false;
		}
		out_damage = hazard.Damage;
		hazard.IsActive = false;
		return true;
	}
	return false;
}
}
