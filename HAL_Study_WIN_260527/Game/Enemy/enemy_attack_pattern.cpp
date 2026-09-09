#include "enemy_attack_pattern_internal.h"

#include "math_utils.h"
#include "game_enemy.h"
#include "Constants/enemy_pattern_constants.h"

namespace EnemyAttackPattern::Internal
{
	// 전투 중 재사용하는 패턴 및 투사체 풀.
	std::array<EnemyPatternRuntime, GameEnemy::ENEMY_CAPACITY> g_EnemyPatterns{};
	std::array<BoneProjectile, EnemyPatternConstants::BoneProjectile::Capacity> g_BoneProjectiles{};
	std::array<DaggerProjectile, EnemyPatternConstants::DaggerProjectile::Capacity> g_DaggerProjectiles{};
	std::array<MageProjectile, EnemyPatternConstants::MageProjectile::Capacity> g_MageProjectiles{};
	std::array<WarriorStrike, EnemyPatternConstants::Warrior::StrikeCapacity> g_WarriorStrikes{};
	std::array<GroundHazard, EnemyPatternConstants::GroundHazard::Capacity> g_GroundHazards{};
	int g_BoneTextureID = TEXTURE_INVALID_ID;
	int g_DaggerTextureID = TEXTURE_INVALID_ID;
	int g_AxeTextureID = TEXTURE_INVALID_ID;
	int g_MageProjectileTextureID = TEXTURE_INVALID_ID;
	int g_WarningTextureID = TEXTURE_INVALID_ID;
	int g_GroundEffectTextureID = TEXTURE_INVALID_ID;
	int g_BoneThrowAudioID = -1;
	std::array<int, 3> g_MageFireAudioIDs{ -1, -1, -1 };
	int g_WarriorSlashAudioID = -1;
	int g_BatDashAudioID = -1;
	int g_ShamanCastAudioID = -1;
} // namespace EnemyAttackPattern::Internal

namespace EnemyAttackPattern
{
	using namespace Internal;

	// 몬스터 종류에 맞는 공격 패턴으로 넘겨주는 진입점.
	void Initialize(const Resources& resources)
	{
		g_BoneTextureID = resources.BoneTextureID;
		g_DaggerTextureID = resources.DaggerTextureID;
		g_AxeTextureID = resources.AxeTextureID;
		g_MageProjectileTextureID = resources.MageProjectileTextureID;
		g_WarningTextureID = resources.WarningTextureID;
		g_GroundEffectTextureID = resources.GroundEffectTextureID;
		g_BoneThrowAudioID = resources.BoneThrowAudioID;
		g_MageFireAudioIDs = resources.MageFireAudioIDs;
		g_WarriorSlashAudioID = resources.WarriorSlashAudioID;
		g_BatDashAudioID = resources.BatDashAudioID;
		g_ShamanCastAudioID = resources.ShamanCastAudioID;
	}

	void Finalize()
	{
		Reset();
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

	void Reset()
	{
		// 라운드가 바뀌면 진행 중이던 공격과 쿨타임을 전부 버린다.
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
		// 스폰 직후 바로 공격하지 않도록 종류별 첫 대기 시간을 준다.
		EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		runtime = EnemyPatternRuntime{};
		runtime.Type = type;
		runtime.IsActive = true;
		// 이 상태만 정하면 이후 전환은 각 enemy_pattern_*.cpp가 맡는다.
		if (type == MonsterType::Slime)
		{
			runtime.State = ActionState::SlimeCooldown;
			runtime.Timer = GetRandomDuration(1.7f, 3.0f);
		}
		else if (IsBoneThrower(type))
		{
			runtime.State = ActionState::SkeletonCooldown;
			runtime.Timer = GetRandomDuration(1.2f, 2.5f);
		}
		else if (type == MonsterType::SkeletonMage)
		{
			runtime.State = ActionState::SkeletonMageCooldown;
			runtime.Timer = GetRandomDuration(1.4f, 2.4f);
		}
		else if (type == MonsterType::Bat)
		{
			runtime.State = ActionState::BatCooldown;
			runtime.Timer = GetRandomDuration(0.9f, 1.8f);
		}
		else if (type == MonsterType::SkeletonRogue)
		{
			runtime.State = ActionState::SkeletonRogueCooldown;
			runtime.Timer = GetRandomDuration(1.0f, 2.0f);
		}
		else if (IsWarriorType(type))
		{
			runtime.State = ActionState::WarriorChase;
			runtime.Timer = GetRandomDuration(0.35f, 0.8f);
		}
		else if (type == MonsterType::Orc)
		{
			runtime.State = ActionState::OrcCooldown;
			runtime.Timer = GetRandomDuration(0.8f, 1.5f);
		}
		else if (IsOrcRogue(type))
		{
			runtime.State = ActionState::RogueApproach;
			runtime.Timer = GetRandomDuration(0.8f, 1.4f);
		}
		else if (type == MonsterType::OrcShaman)
		{
			runtime.State = ActionState::ShamanCooldown;
			runtime.Timer = GetRandomDuration(1.5f, 2.8f);
		}
		else if (type == MonsterType::BossCthulhu)
		{
			runtime.State = ActionState::CthulhuRoam;
			runtime.Timer = GetRandomDuration(EnemyPatternConstants::Cthulhu::RoamMinDuration,
			                                  EnemyPatternConstants::Cthulhu::RoamMaxDuration);
			runtime.StrafeDirection = RandomSigned() < 0.0f ? -1.0f : 1.0f;
		}
	}

	void OnDeactivate(int enemy_id)
	{
		if (IsValidEnemyID(enemy_id))
		{
			g_EnemyPatterns[enemy_id] = EnemyPatternRuntime{};
		}
	}

	void UpdateEnemy(int enemy_id, MonsterType type, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position, float visual_bottom_offset, float animation_elapsed)
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
			trail.TimeRemaining = std::max(0.0f, trail.TimeRemaining - safe_delta_time);
		}
		// 공통 생존 처리가 끝난 뒤 몬스터 종류별 상태 머신으로 넘긴다.
		if (type == MonsterType::Slime)
		{
			UpdateSlime(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (IsBoneThrower(type))
		{
			UpdateSkeleton(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (type == MonsterType::SkeletonMage)
		{
			UpdateSkeletonMage(enemy_id, runtime, enemy, safe_delta_time, player_position, visual_bottom_offset);
		}
		else if (type == MonsterType::Bat)
		{
			UpdateBat(enemy_id, runtime, enemy, safe_delta_time, player_position, animation_elapsed);
		}
		else if (type == MonsterType::SkeletonRogue)
		{
			UpdateSkeletonRogue(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (type == MonsterType::Orc)
		{
			UpdateOrc(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (IsOrcRogue(type))
		{
			UpdateRogue(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (type == MonsterType::OrcShaman)
		{
			UpdateShaman(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (IsWarriorType(type))
		{
			UpdateWarrior(enemy_id, runtime, enemy, safe_delta_time, player_position);
		}
		else if (type == MonsterType::BossCthulhu)
		{
			UpdateCthulhu(enemy_id, runtime, enemy, safe_delta_time, player_position);
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
		    (runtime.State == ActionState::BatWindup || runtime.State == ActionState::BatDash))
		{
			return runtime.AnimationElapsed;
		}
		return default_elapsed;
	}

	bool GetAnimationFrameRegion(int enemy_id, int& out_frame_x, int& out_frame_y)
	{
		static constexpr int FrameHeight = 112;
		static constexpr int FrameWidth = 192;
		static constexpr int AttackTwoFrameCount = 9;
		static constexpr int AttackOneFrameCount = 7;
		static constexpr int FlyFrameCount = 6;
		static constexpr int WalkFrameCount = 12;
		static constexpr int IdleFrameCount = 15;
		static constexpr int AttackTwoRow = 4;
		static constexpr int AttackOneRow = 3;
		static constexpr int FlyRow = 2;
		static constexpr int WalkRow = 1;
		static constexpr int IdleRow = 0;
		static constexpr float AnimationFrameDuration = 0.09f;
		if (!IsValidEnemyID(enemy_id))
		{
			return false;
		}
		const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		if (!runtime.IsActive || runtime.Type != MonsterType::BossCthulhu)
		{
			return false;
		}
		int row = IdleRow;
		int frame_count = IdleFrameCount;
		bool loop = true;
		switch (runtime.State)
		{
		case ActionState::CthulhuRoam:
			row = WalkRow;
			frame_count = WalkFrameCount;
			break;
		case ActionState::CthulhuCast:
			if ((runtime.MovementStep & 1) == 0)
			{
				row = AttackOneRow;
				frame_count = AttackOneFrameCount;
			}
			else
			{
				row = AttackTwoRow;
				frame_count = AttackTwoFrameCount;
			}
			loop = false;
			break;
		case ActionState::CthulhuVanish:
		case ActionState::CthulhuDashWindup:
		case ActionState::CthulhuDash:
			row = FlyRow;
			frame_count = FlyFrameCount;
			break;
		case ActionState::CthulhuRecovery:
		default:
			break;
		}
		const int raw_frame = static_cast<int>(runtime.AnimationElapsed / AnimationFrameDuration);
		const int frame = loop ? raw_frame % frame_count : std::min(raw_frame, frame_count - 1);
		out_frame_x = frame * FrameWidth;
		out_frame_y = row * FrameHeight;
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
		if (runtime.Type == MonsterType::BossCthulhu)
		{
			if (runtime.State == ActionState::CthulhuVanish)
			{
				return Saturate(runtime.Timer / EnemyPatternConstants::Cthulhu::VanishDuration);
			}
			if (runtime.State == ActionState::CthulhuDashWindup)
			{
				return 1.0f - Saturate(runtime.Timer / EnemyPatternConstants::Cthulhu::DashWindupDuration);
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
		static constexpr float CastTelegraphDuration = 0.42f;
		if (!IsValidEnemyID(enemy_id))
		{
			return true;
		}
		const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		return !runtime.IsActive || runtime.Type != MonsterType::BossCthulhu ||
		       (runtime.State == ActionState::CthulhuCast && runtime.AnimationElapsed >= CastTelegraphDuration);
	}

	bool IsCthulhuDashing(int enemy_id)
	{
		if (!IsValidEnemyID(enemy_id))
		{
			return false;
		}
		const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		return runtime.IsActive && runtime.Type == MonsterType::BossCthulhu &&
		       runtime.State == ActionState::CthulhuDash;
	}

	int GetBossAttackVariant(int enemy_id)
	{
		if (!IsValidEnemyID(enemy_id))
		{
			return 0;
		}
		const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		return runtime.IsActive && runtime.Type == MonsterType::BossCthulhu
		           ? runtime.MovementStep % EnemyPatternConstants::Cthulhu::PatternCount
		           : 0;
	}

	int GetDashAfterimagePaths(int enemy_id, DashAfterimagePath* out_paths, int capacity)
	{
		if (!IsValidEnemyID(enemy_id) || !out_paths || capacity <= 0)
		{
			return 0;
		}
		const EnemyPatternRuntime& runtime = g_EnemyPatterns[enemy_id];
		if (!runtime.IsActive || runtime.Type != MonsterType::OrcWarrior)
		{
			return 0;
		}
		int path_count = 0;
		for (const WarriorDashTrailRuntime& trail : runtime.WarriorDashTrails)
		{
			const float path_x = trail.End.x - trail.Start.x;
			const float path_y = trail.End.y - trail.Start.y;
			if (trail.TimeRemaining <= 0.0f || path_count >= capacity || path_x * path_x + path_y * path_y <= 1.0f)
			{
				continue;
			}
			out_paths[path_count++] = {
				trail.Start,
				trail.End,
				Saturate(trail.TimeRemaining / EnemyPatternConstants::OrcWarrior::DashTrailDuration),
			};
		}
		return path_count;
	}
} // namespace EnemyAttackPattern
