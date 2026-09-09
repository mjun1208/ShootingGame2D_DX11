#ifndef ENEMY_ATTACK_PATTERN_INTERNAL_H
#define ENEMY_ATTACK_PATTERN_INTERNAL_H

#include "enemy_attack_pattern.h"
#include "Constants/enemy_pattern_constants.h"

#include "Audio.h"
#include "collision.h"
#include "enemy.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "random_utils.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "trail.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace EnemyAttackPattern::Internal
{
	// 패턴 파일들이 같이 쓰는 상수와 자료형.

	enum class ActionState : std::uint8_t
	{
		// 이름은 몬스터 종류 + 현재 행동 순서로 맞춘다.
		None,
		SlimeCooldown,
		SlimeTelegraph,
		SlimeDash,
		SkeletonCooldown,
		SkeletonWindup,
		SkeletonMageCooldown,
		SkeletonMageWindup,
		BatCooldown,
		BatWindup,
		BatDash,
		OrcCooldown,
		OrcWindup,
		RogueApproach,
		RogueWindup,
		RogueDash,
		SkeletonRogueCooldown,
		SkeletonRogueWindup,
		WarriorChase,
		WarriorSidestep,
		WarriorWindup,
		WarriorRecovery,
		ShamanCooldown,
		ShamanCast,
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
		// 적 한 마리의 상태 머신 진행값
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
		float StrafeDirection{ 1.0f };
		int MovementStep{ 0 };
		std::array<WarriorDashTrailRuntime, EnemyPatternConstants::OrcWarrior::DashTrailCount> WarriorDashTrails{};
		bool IsActive{ false };
	};

	struct BoneProjectile
	{
		// 투사체 풀은 IsActive가 false인 칸을 찾아 다시 쓴다.
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Travelled{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ EnemyPatternConstants::BoneProjectile::SpinSpeed };
		bool IsActive{ false };
	};

	struct DaggerProjectile
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Travelled{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		float MaxDistance{ EnemyPatternConstants::DaggerProjectile::MaxDistance };
		float Damage{ EnemyPatternConstants::DaggerProjectile::Damage };
		float Radius{ EnemyPatternConstants::DaggerProjectile::Radius };
		float Size{ EnemyPatternConstants::DaggerProjectile::Size };
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
		float Damage{ EnemyPatternConstants::SkeletonWarrior::StrikeDamage };
		float Radius{ EnemyPatternConstants::Warrior::StrikeRadius };
		float Lifetime{ EnemyPatternConstants::Warrior::StrikeLifetime };
		bool IsActive{ false };
	};

	struct GroundHazard
	{
		DirectX::XMFLOAT2 Position{};
		float Elapsed{ 0.0f };
		float Lifetime{ EnemyPatternConstants::Shaman::HazardLifetime };
		float Radius{ EnemyPatternConstants::Shaman::HazardRadius };
		float Damage{ EnemyPatternConstants::Shaman::HazardDamage };
		bool IsActive{ false };
	};

	// 실제 변수는 enemy_attack_pattern_state.cpp에 모아 둔다.
	extern std::array<EnemyPatternRuntime, GameEnemy::ENEMY_CAPACITY> g_EnemyPatterns;
	extern std::array<BoneProjectile, EnemyPatternConstants::BoneProjectile::Capacity> g_BoneProjectiles;
	extern std::array<DaggerProjectile, EnemyPatternConstants::DaggerProjectile::Capacity> g_DaggerProjectiles;
	extern std::array<MageProjectile, EnemyPatternConstants::MageProjectile::Capacity> g_MageProjectiles;
	extern std::array<WarriorStrike, EnemyPatternConstants::Warrior::StrikeCapacity> g_WarriorStrikes;
	extern std::array<GroundHazard, EnemyPatternConstants::GroundHazard::Capacity> g_GroundHazards;
	extern int g_BoneTextureID;
	extern int g_DaggerTextureID;
	extern int g_AxeTextureID;
	extern int g_MageProjectileTextureID;
	extern int g_WarningTextureID;
	extern int g_GroundEffectTextureID;
	extern int g_BoneThrowAudioID;
	extern std::array<int, 3> g_MageFireAudioIDs;
	extern int g_WarriorSlashAudioID;
	extern int g_BatDashAudioID;
	extern int g_ShamanCastAudioID;

	using GameEnemy::IsValidEnemyID;

	inline bool IsBoneThrower(MonsterType type)
	{
		return type == MonsterType::SkeletonBase;
	}

	inline bool IsOrcRogue(MonsterType type)
	{
		return type == MonsterType::OrcRogue;
	}

	inline bool IsWarriorType(MonsterType type)
	{
		return type == MonsterType::SkeletonWarrior || type == MonsterType::OrcWarrior;
	}

	inline float GetRandomDuration(float minimum, float maximum)
	{
		return RandomFloat(minimum, maximum);
	}

	inline DirectX::XMFLOAT2 GetAttackOrigin(const cEnemy& enemy)
	{
		return enemy.GetMapCollisionCenter();
	}

	inline void UpdateRangedMovement(cEnemy& enemy, float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
		float speed_scale = 0.0f;
		if (distance_squared < EnemyPatternConstants::RangedMovement::RetreatDistance *
		                           EnemyPatternConstants::RangedMovement::RetreatDistance)
		{
			speed_scale = -1.05f;
		}
		else if (distance_squared > EnemyPatternConstants::RangedMovement::ApproachDistance *
		                                EnemyPatternConstants::RangedMovement::ApproachDistance)
		{
			speed_scale = 0.72f;
		}
		enemy.Update(delta_time, player_position, speed_scale);
	}

	// 할당 순서를 유지하며 첫 번째 빈 슬롯을 반환한다.
	template <typename T, std::size_t N> T* FindInactiveSlot(std::array<T, N>& slots)
	{
		const auto slot = std::find_if(slots.begin(), slots.end(),
		                               [](const T& value)
		                               {
			                               return !value.IsActive;
		                               });
		return slot == slots.end() ? nullptr : &*slot;
	}

	inline bool SpawnBoneProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction, int enemy_id)
	{
		BoneProjectile* available = FindInactiveSlot(g_BoneProjectiles);
		if (!available)
		{
			return false;
		}
		BoneProjectile& projectile = *available;

		projectile = BoneProjectile{};
		projectile.Position = position;
		projectile.Velocity = {
			direction.x * EnemyPatternConstants::BoneProjectile::Speed,
			direction.y * EnemyPatternConstants::BoneProjectile::Speed,
		};
		projectile.Rotation = std::atan2(direction.y, direction.x);
		projectile.AngularVelocity = (enemy_id & 1) == 0 ? EnemyPatternConstants::BoneProjectile::SpinSpeed
		                                                 : -EnemyPatternConstants::BoneProjectile::SpinSpeed;
		projectile.IsActive = true;
		if (g_BoneThrowAudioID >= 0)
		{
			Audio_Play(g_BoneThrowAudioID);
		}
		return true;
	}

	inline bool SpawnMageProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction,
	                                int audio_variant)
	{
		MageProjectile* available = FindInactiveSlot(g_MageProjectiles);
		if (!available)
		{
			return false;
		}
		MageProjectile& projectile = *available;

		projectile = MageProjectile{};
		projectile.Position = position;
		projectile.Velocity = {
			direction.x * EnemyPatternConstants::MageProjectile::Speed,
			direction.y * EnemyPatternConstants::MageProjectile::Speed,
		};
		projectile.Rotation = std::atan2(direction.y, direction.x) + DirectX::XM_PI;
		projectile.IsActive = true;
		const std::size_t audio_index =
		    static_cast<std::size_t>(std::max(audio_variant, 0)) % g_MageFireAudioIDs.size();
		const int fire_audio_id = g_MageFireAudioIDs[audio_index];
		if (fire_audio_id >= 0)
		{
			Audio_Play(fire_audio_id);
		}
		return true;
	}

	inline bool SpawnWarriorStrike(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction,
	                               MonsterType type)
	{
		WarriorStrike* available = FindInactiveSlot(g_WarriorStrikes);
		if (!available)
		{
			return false;
		}
		WarriorStrike& strike = *available;

		strike = WarriorStrike{};
		strike.Position = position;
		strike.Damage = type == MonsterType::OrcWarrior ? EnemyPatternConstants::OrcWarrior::StrikeDamage
		                                                : EnemyPatternConstants::SkeletonWarrior::StrikeDamage;
		strike.IsActive = true;

		const float effect_scale = type == MonsterType::OrcWarrior ? 1.14f : 1.0f;
		cGameEffectManager::GetInstance().Play(GameEffectType::EnemyWarriorSlash, position, effect_scale,
		                                       { 1.0f, 0.05f, 0.02f, 1.0f }, std::atan2(direction.y, direction.x));
		if (g_WarriorSlashAudioID >= 0)
		{
			Audio_Play(g_WarriorSlashAudioID);
		}
		return true;
	}

	inline bool SpawnRogueStrike(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction)
	{
		WarriorStrike* available = FindInactiveSlot(g_WarriorStrikes);
		if (!available)
		{
			return false;
		}
		WarriorStrike& strike = *available;

		strike = WarriorStrike{};
		strike.Position = position;
		strike.Damage = EnemyPatternConstants::Rogue::StrikeDamage;
		strike.Radius = EnemyPatternConstants::Rogue::StrikeRadius;
		strike.Lifetime = 0.10f;
		strike.IsActive = true;

		const float angle = std::atan2(direction.y, direction.x) + (-0.10f);
		cGameEffectManager::GetInstance().Play(GameEffectType::DashSlashHitCut, position, 0.62f,
		                                       DirectX::XMFLOAT4{ 0.88f, 0.12f, 0.05f, 0.94f }, angle);
		if (g_WarriorSlashAudioID >= 0)
		{
			Audio_Play(g_WarriorSlashAudioID);
		}
		return true;
	}

	inline void BeginOrcWarriorDashTrail(EnemyPatternRuntime& runtime, const DirectX::XMFLOAT2& position)
	{
		WarriorDashTrailRuntime& trail = runtime.WarriorDashTrails[static_cast<std::size_t>(runtime.MovementStep) %
		                                                           EnemyPatternConstants::OrcWarrior::DashTrailCount];
		trail.Start = position;
		trail.End = position;
		trail.TimeRemaining = EnemyPatternConstants::OrcWarrior::DashTrailDuration;
		if (g_BatDashAudioID >= 0)
		{
			Audio_Play(g_BatDashAudioID);
		}
	}

	inline DirectX::XMFLOAT2 GetOrcWarriorSidestepDirection(const DirectX::XMFLOAT2& position,
	                                                        const DirectX::XMFLOAT2& player_position,
	                                                        float strafe_direction)
	{
		const DirectX::XMFLOAT2 forward = GetDirection(position, player_position);
		return {
			forward.x * EnemyPatternConstants::OrcWarrior::SidestepForwardWeight -
			    forward.y * strafe_direction * EnemyPatternConstants::OrcWarrior::SidestepSideWeight,
			forward.y * EnemyPatternConstants::OrcWarrior::SidestepForwardWeight +
			    forward.x * strafe_direction * EnemyPatternConstants::OrcWarrior::SidestepSideWeight,
		};
	}

	inline void BeginOrcWarriorSidestep(EnemyPatternRuntime& runtime, const cEnemy& enemy)
	{
		runtime.State = ActionState::WarriorSidestep;
		runtime.Timer = EnemyPatternConstants::OrcWarrior::SidestepDuration;
		runtime.MovementStep = 0;
		runtime.StrafeDirection = RandomSigned() < 0.0f ? -1.0f : 1.0f;
		BeginOrcWarriorDashTrail(runtime, enemy.GetPosition());
	}

	inline bool SpawnGroundHazard(const DirectX::XMFLOAT2& position)
	{
		GroundHazard* available = FindInactiveSlot(g_GroundHazards);
		if (!available)
		{
			return false;
		}
		GroundHazard& hazard = *available;

		hazard = GroundHazard{};
		hazard.Position = position;
		hazard.IsActive = true;
		if (g_ShamanCastAudioID >= 0)
		{
			Audio_Play(g_ShamanCastAudioID);
		}
		return true;
	}

	inline bool SpawnDaggerProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction)
	{
		DaggerProjectile* available = FindInactiveSlot(g_DaggerProjectiles);
		if (!available)
		{
			return false;
		}
		DaggerProjectile& projectile = *available;

		projectile = DaggerProjectile{};
		projectile.Position = position;
		projectile.Velocity = {
			direction.x * EnemyPatternConstants::DaggerProjectile::Speed,
			direction.y * EnemyPatternConstants::DaggerProjectile::Speed,
		};
		projectile.Rotation = std::atan2(direction.y, direction.x) + DirectX::XM_PIDIV4;
		projectile.IsActive = true;
		return true;
	}

	inline bool SpawnAxeProjectile(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction, int enemy_id)
	{
		DaggerProjectile* available = FindInactiveSlot(g_DaggerProjectiles);
		if (!available)
		{
			return false;
		}
		DaggerProjectile& projectile = *available;

		projectile = DaggerProjectile{};
		projectile.Position = position;
		projectile.Velocity = {
			direction.x * EnemyPatternConstants::OrcAxe::ProjectileSpeed,
			direction.y * EnemyPatternConstants::OrcAxe::ProjectileSpeed,
		};
		projectile.Rotation = std::atan2(direction.y, direction.x);
		projectile.AngularVelocity = (enemy_id & 1) == 0 ? EnemyPatternConstants::OrcAxe::ProjectileSpinSpeed
		                                                 : -EnemyPatternConstants::OrcAxe::ProjectileSpinSpeed;
		projectile.MaxDistance = EnemyPatternConstants::OrcAxe::ProjectileMaxDistance;
		projectile.Damage = EnemyPatternConstants::OrcAxe::ProjectileDamage;
		projectile.Radius = EnemyPatternConstants::OrcAxe::ProjectileRadius;
		projectile.Size = EnemyPatternConstants::OrcAxe::ProjectileSize;
		projectile.IsAxe = true;
		projectile.IsActive = true;
		if (g_BatDashAudioID >= 0)
		{
			Audio_Play(g_BatDashAudioID);
		}
		return true;
	}

	inline void FireDaggerFan(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& aim_direction,
	                          float muzzle_offset)
	{
		for (int dagger_index = -1; dagger_index <= 1; ++dagger_index)
		{
			const DirectX::XMFLOAT2 direction =
			    RotateDirection(aim_direction, static_cast<float>(dagger_index) *
			                                       EnemyPatternConstants::SkeletonRogue::DaggerAngleStep);
			SpawnDaggerProjectile(
			    {
			        position.x + direction.x * muzzle_offset,
			        position.y + direction.y * muzzle_offset,
			    },
			    direction);
		}
	}

	void UpdateSlime(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position);
	void UpdateSkeleton(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                    const DirectX::XMFLOAT2& player_position);
	void UpdateSkeletonMage(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                        const DirectX::XMFLOAT2& player_position, float visual_bottom_offset);
	void UpdateBat(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	               const DirectX::XMFLOAT2& player_position, float animation_elapsed);
	void UpdateSkeletonRogue(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                         const DirectX::XMFLOAT2& player_position);
	void UpdateWarrior(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                   const DirectX::XMFLOAT2& player_position);
	void UpdateOrc(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	               const DirectX::XMFLOAT2& player_position);
	void UpdateRogue(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position);
	void UpdateShaman(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                  const DirectX::XMFLOAT2& player_position);
	void UpdateCthulhu(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                   const DirectX::XMFLOAT2& player_position);

} // namespace EnemyAttackPattern::Internal

#endif // ENEMY_ATTACK_PATTERN_INTERNAL_H
