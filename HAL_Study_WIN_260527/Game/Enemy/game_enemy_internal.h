#ifndef GAME_ENEMY_INTERNAL_H
#define GAME_ENEMY_INTERNAL_H

#include "game_enemy.h"
#include "Constants/player_constants.h"
#include "Constants/enemy_constants.h"

#include "Audio.h"
#include "blood.h"
#include "chain_lightning.h"
#include "collision.h"
#include "enemy.h"
#include "enemy_attack_pattern.h"
#include "game_bullet.h"
#include "game_damage_text.h"
#include "game_data_manager.h"
#include "game_effect.h"
#include "game_experience_gem.h"
#include "game_healing_item.h"
#include "game_player.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "projectile.h"
#include "random_utils.h"
#include "slime_goo.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace GameEnemy::Internal
{
	// 적 관련 cpp에서 같이 쓰는 상수와 자료형.

	// 보스의 여러 겹 탄막이 동시에 남아 있어 넉넉한 크기가 필요하다.

	// 2페이즈부터 조준 방향 양쪽으로 젤리탄을 발사한다.

	inline bool IsBossType(MonsterType type)
	{
		return type == MonsterType::BossSlime || type == MonsterType::BossCorruptedKnight ||
		       type == MonsterType::BossCorruptedMage || type == MonsterType::BossCthulhu;
	}

	inline bool IsCorruptedBossType(MonsterType type)
	{
		return type == MonsterType::BossCorruptedKnight || type == MonsterType::BossCorruptedMage ||
		       type == MonsterType::BossCthulhu;
	}

	inline bool IsOrcType(MonsterType type)
	{
		return type == MonsterType::Orc || type == MonsterType::OrcRogue || type == MonsterType::OrcShaman ||
		       type == MonsterType::OrcWarrior;
	}

	inline MonsterType GetBossTypeForRound(int round)
	{
		switch (round)
		{
		case 1:
			return MonsterType::BossSlime;
		case 2:
			return MonsterType::BossCorruptedKnight;
		case 3:
			return MonsterType::BossCorruptedMage;
		default:
			return MonsterType::BossCthulhu;
		}
	}

	inline float GetSpawnTelegraphSize(MonsterType type)
	{
		if (!IsBossType(type))
		{
			return EnemyConstants::Spawn::TelegraphSize;
		}

		const DirectX::XMFLOAT2 boss_draw_size = GetMonsterData(type).DrawSize;
		return std::max(EnemyConstants::Spawn::TelegraphSize, std::min(boss_draw_size.x, boss_draw_size.y));
	}

	inline constexpr int BONE_DROP_FRAME_COUNT = 3;
	inline constexpr int BONE_DROP_FRAME_WIDTH = 16;
	inline constexpr int BONE_DROP_FRAME_HEIGHT = 16;
	inline constexpr int BONE_BURST_PARTICLE_COUNT = 8;
	inline constexpr float BONE_BURST_GRAVITY = 280.0f;
	inline constexpr float BONE_BURST_DRAG_PER_SECOND = 0.18f;
	inline constexpr float BONE_DROP_TEXTURE_WIDTH = 48.0f;
	inline constexpr float BONE_DROP_TEXTURE_HEIGHT = 16.0f;
	inline constexpr int CUT_DUST_GRID_SIZE = 8;
	inline constexpr int CUT_DUST_MIN_PARTICLE_COUNT = 32;
	inline constexpr int CUT_DUST_MAX_PARTICLE_COUNT = 72;
	inline constexpr int CUT_DUST_CAPACITY = 8192;
	inline constexpr float CUT_DUST_GRAVITY = 240.0f;
	inline constexpr float CUT_DUST_DRAG_PER_SECOND = 0.16f;

	enum class RoomWaveState : std::uint8_t
	{
		Dormant,
		Waiting,
		Telegraphing,
		Active,
		Cleared,
	};

	struct RoomWaveRuntime
	{
		RoomWaveState State{ RoomWaveState::Dormant };
		int WaveIndex{ 0 };
		int WaveCount{ 0 };
		int TotalEnemyCount{ 0 };
		int SpawnRetryCount{ 0 };
		float DelayRemaining{ 0.0f };
	};

	struct EnemySlot
	{
		cEnemy Entity{};
		MonsterType Type{ EnemyConstants::Spawn::DefaultMonsterType };
		int RoomIndex{ -1 };
		float DrawScale{ 1.0f };
		float ExperienceScale{ 1.0f };
		float BossFireScaleElapsed{ EnemyConstants::BossJelly::FireScaleDuration };
		float BossSplitScaleElapsed{ EnemyConstants::BossBody::SplitScaleDuration };
	};

	inline void ApplyCombatKnockback(EnemySlot& slot, const DirectX::XMFLOAT2& direction, float speed)
	{
		if (!IsBossType(slot.Type))
		{
			slot.Entity.ApplyKnockback(direction, speed);
		}
	}

	struct MonsterLightStyle
	{
		DirectX::XMFLOAT3 Color{ 1.0f, 1.0f, 1.0f };
		float Radius{ 160.0f };
		float Strength{ 0.25f };
	};

	struct MonsterMapCollisionShape
	{
		DirectX::XMFLOAT2 Offset{};
		float Radius{ 0.0f };
	};

	struct PendingEnemySpawn
	{
		DirectX::XMFLOAT2 Position{};
		float Speed{ 0.0f };
		float Elapsed{ 0.0f };
		MonsterType Type{ EnemyConstants::Spawn::DefaultMonsterType };
		int RoomIndex{ -1 };
	};

	struct SpawnPlacement
	{
		DirectX::XMFLOAT2 Position{};
		float CollisionRadius{ 0.0f };
	};

	struct SpawnArrivalEffect
	{
		DirectX::XMFLOAT2 Position{};
		float Elapsed{ 0.0f };
		float BaseSize{ EnemyConstants::Spawn::TelegraphSize };
	};

	struct BoneDropEffect
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Size{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		float Elapsed{ 0.0f };
		float Lifetime{ 0.0f };
		int Frame{ 0 };
	};

	struct CutDustParticle
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		DirectX::XMFLOAT2 Size{};
		DirectX::XMFLOAT2 TexcoordOffset{};
		DirectX::XMFLOAT2 TexcoordScale{};
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		float Elapsed{ 0.0f };
		float Lifetime{ 0.0f };
		MonsterType Type{ EnemyConstants::Spawn::DefaultMonsterType };
	};

	enum class BossProjectileStyle : std::uint8_t
	{
		Jelly,
		Knight,
		MageViolet,
		MageAzure,
		Abyss,
	};

	struct BossJellyBullet
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float Radius{ 0.0f };
		float Size{ 0.0f };
		float Damage{ 0.0f };
		float Travelled{ 0.0f };
		float MaxDistance{ 0.0f };
		float Rotation{ 0.0f };
		float TurnRate{ 0.0f };
		float Acceleration{ 0.0f };
		float VisualScale{ 1.0f };
		bool IsPrimary{ false };
		BossProjectileStyle Style{ BossProjectileStyle::Jelly };
		bool IsActive{ false };
	};

	enum class CthulhuEyeLaserPhase : std::uint8_t
	{
		Inactive,
		Telegraph,
		Firing,
	};

	struct CthulhuEyeLaser
	{
		DirectX::XMFLOAT2 Start{};
		DirectX::XMFLOAT2 End{};
		DirectX::XMFLOAT2 Direction{ 1.0f, 0.0f };
		float Timer{ 0.0f };
		float PhaseDuration{ 0.0f };
		bool DamageApplied{ false };
		CthulhuEyeLaserPhase Phase{ CthulhuEyeLaserPhase::Inactive };
	};

	struct PendingDashSlashTarget
	{
		int EnemyID{ -1 };
		DirectX::XMFLOAT2 LastPosition{};
		bool RewardGranted{ false };
	};

	struct PendingDashSlashAttack
	{
		std::vector<PendingDashSlashTarget> Targets;
		DirectX::XMFLOAT2 Direction{};
		float Damage{ 0.0f };
		float Elapsed{ 0.0f };
		int NextHitIndex{ 0 };
	};

	struct EnemyCollisionGrid
	{
		int BucketHeads[EnemyConstants::Collision::GridBucketCount]{};
		int NextEnemy[GameEnemy::ENEMY_CAPACITY]{};
		int CellX[GameEnemy::ENEMY_CAPACITY]{};
		int CellY[GameEnemy::ENEMY_CAPACITY]{};
		int MinCellX{};
		int MaxCellX{};
		int MinCellY{};
		int MaxCellY{};
		bool HasAliveEnemy{ false };
	};

	struct EnemySystemState
	{
		// 적 본체와 방 진행 상태
		EnemySlot EnemySlots[GameEnemy::ENEMY_CAPACITY]{};
		EnemyCollisionGrid CollisionGrid{};
		std::vector<RoomWaveRuntime> RoomWaves;
		std::vector<bool> DiscoveredRooms;
		std::vector<PendingEnemySpawn> PendingEnemySpawns;

		// 잠깐 보이고 사라지는 전투 연출
		std::vector<SpawnArrivalEffect> SpawnArrivalEffects;
		std::vector<BoneDropEffect> BoneDropEffects;
		std::vector<CutDustParticle> CutDustParticles;
		std::vector<PendingDashSlashAttack> PendingDashSlashAttacks;

		// 적 효과음
		std::array<int, PlayerConstants::Slash::CutCount> DashSlashHitAudioIDs = []
		{
			std::array<int, PlayerConstants::Slash::CutCount> audio_ids{};
			audio_ids.fill(-1);
			return audio_ids;
		}();
		int SlimeDamageAudioID{ -1 };
		int SlimeDeathAudioID{ -1 };
		int BossSlimeProjectileFireAudioID{ -1 };
		int BatDamageAudioID{ -1 };
		int BatDeathAudioID{ -1 };
		int BatDashAudioID{ -1 };
		int BonesDamageAudioID{ -1 };
		int BonesDeathAudioID{ -1 };
		std::array<int, 3> SkeletonMageFireAudioIDs{ -1, -1, -1 };
		int EnemyWarriorSlashAudioID{ -1 };
		std::array<int, EnemyConstants::Audio::DamageSoundPaths.size()> OrcDamageAudioIDs = []
		{
			std::array<int, EnemyConstants::Audio::DamageSoundPaths.size()> audio_ids{};
			audio_ids.fill(-1);
			return audio_ids;
		}();
		int OrcDeathAudioID{ -1 };
		int OrcShamanCastAudioID{ -1 };
		int CthulhuLaserAudioID{ -1 };

		// 몬스터와 공격 연출 이미지
		std::array<int, MONSTER_TYPE_COUNT> MonsterTextureIDs = []
		{
			std::array<int, MONSTER_TYPE_COUNT> texture_ids{};
			texture_ids.fill(TEXTURE_INVALID_ID);
			return texture_ids;
		}();
		int BoneDropTextureID{ TEXTURE_INVALID_ID };
		int DaggerTextureID{ TEXTURE_INVALID_ID };
		int AxeTextureID{ TEXTURE_INVALID_ID };
		int MageProjectileTextureID{ TEXTURE_INVALID_ID };
		int SpawnTelegraphTextureID{ TEXTURE_INVALID_ID };
		int BossJellyTextureID{ TEXTURE_INVALID_ID };
		int BossJellyTelegraphTextureID{ TEXTURE_INVALID_ID };
		int CorruptionProjectileTextureID{ TEXTURE_INVALID_ID };
		int CorruptionAuraTextureID{ TEXTURE_INVALID_ID };
		int CthulhuEyeChargeTextureID{ TEXTURE_INVALID_ID };
		int CthulhuEyeLaserTextureID{ TEXTURE_INVALID_ID };

		// 보스 패턴 진행 상태
		std::array<BossJellyBullet, EnemyConstants::BossJelly::BulletMax> BossJellyBullets{};
		float BossJellyFireCooldown{ EnemyConstants::BossJelly::FireInterval };
		int BossJellyTelegraphEnemyID{ -1 };
		DirectX::XMFLOAT2 BossJellyTelegraphDirection{ 1.0f, 0.0f };
		int BossSplitStage{ 0 };
		int BossVolleySequence{ 0 };
		float CorruptionFireCooldown{ 0.8f };
		int CorruptionVolleySequence{ 0 };
		int CthulhuActivePattern{ -1 };
		int CthulhuPatternStep{ 0 };
		CthulhuEyeLaser CthulhuLaser{};
		float CthulhuDashBulletCooldown{ 0.0f };
		int CthulhuDashBulletSequence{ 0 };

		// 프레임 전체에서 같이 쓰는 값
		std::array<DirectX::XMUINT2, MONSTER_TYPE_COUNT> MonsterTextureSizes{};
		int CurrentRoomIndex{ -1 };
		bool RoundCleared{ false };
		float MonsterAnimationElapsed{ 0.0f };
		float EnemyCollisionCellSize{ cEnemy::RADIUS * 2.0f };
		GameEnemy::BossDefeatedEvent PendingBossDefeat{};
		bool BossDefeatedEventPending{ false };
		GameEnemy::CombatFeedback CombatFeedback{};
	};

	// 실제 변수는 game_enemy_state.cpp에 한 번만 만든다.
	extern EnemySystemState g_EnemySystem;

	inline void RecordCombatFeedback(const EnemySlot& slot, const DirectX::XMFLOAT2& position,
	                                 const DirectX::XMFLOAT2& direction, float damage, bool was_killed,
	                                 bool heavy_hit = false)
	{
		++g_EnemySystem.CombatFeedback.HitCount;
		g_EnemySystem.CombatFeedback.KillCount += was_killed ? 1 : 0;
		g_EnemySystem.CombatFeedback.HeavyHit = g_EnemySystem.CombatFeedback.HeavyHit || heavy_hit;
		g_EnemySystem.CombatFeedback.BossHit = g_EnemySystem.CombatFeedback.BossHit || IsBossType(slot.Type);
		g_EnemySystem.CombatFeedback.BossKilled =
		    g_EnemySystem.CombatFeedback.BossKilled || (was_killed && IsBossType(slot.Type));

		if (damage >= g_EnemySystem.CombatFeedback.StrongestDamage || was_killed)
		{
			g_EnemySystem.CombatFeedback.StrongestDamage =
			    std::max(g_EnemySystem.CombatFeedback.StrongestDamage, damage);
			g_EnemySystem.CombatFeedback.Position = position;
			g_EnemySystem.CombatFeedback.Direction = direction;
		}
	}

	inline MonsterLightStyle GetMonsterLightStyle(MonsterType type)
	{
		switch (type)
		{
		case MonsterType::Slime:
			return { { 0.20f, 1.0f, 0.30f }, 175.0f, 0.30f };
		case MonsterType::SkeletonMage:
			return { { 0.52f, 0.24f, 1.0f }, 220.0f, 0.43f };
		case MonsterType::SkeletonRogue:
			return { { 0.18f, 0.58f, 1.0f }, 180.0f, 0.28f };
		case MonsterType::SkeletonWarrior:
			return { { 1.0f, 0.42f, 0.16f }, 190.0f, 0.32f };
		case MonsterType::Bat:
			return { { 0.66f, 0.22f, 1.0f }, 155.0f, 0.27f };
		case MonsterType::Orc:
			return { { 0.62f, 0.86f, 0.18f }, 185.0f, 0.25f };
		case MonsterType::OrcRogue:
			return { { 1.0f, 0.18f, 0.12f }, 185.0f, 0.31f };
		case MonsterType::OrcShaman:
			return { { 0.38f, 1.0f, 0.34f }, 225.0f, 0.44f };
		case MonsterType::OrcWarrior:
			return { { 1.0f, 0.32f, 0.10f }, 205.0f, 0.36f };
		case MonsterType::BossSlime:
			return { { 1.0f, 0.12f, 0.18f }, 360.0f, 0.62f };
		case MonsterType::BossCorruptedKnight:
			return { { 0.38f, 0.02f, 0.72f }, 390.0f, 0.74f };
		case MonsterType::BossCorruptedMage:
			return { { 0.52f, 0.06f, 0.96f }, 430.0f, 0.82f };
		case MonsterType::BossCthulhu:
			return { { 0.24f, 0.01f, 0.52f }, 520.0f, 0.92f };
		case MonsterType::SkeletonBase:
		default:
			return { { 0.48f, 0.68f, 1.0f }, 175.0f, 0.24f };
		}
	}

	inline DirectX::XMFLOAT4 GetBossProjectileCoreColor(BossProjectileStyle style)
	{
		switch (style)
		{
		case BossProjectileStyle::Knight:
			return { 1.0f, 0.16f, 0.035f, 0.98f };
		case BossProjectileStyle::MageViolet:
			return { 0.72f, 0.12f, 1.0f, 0.97f };
		case BossProjectileStyle::MageAzure:
			return { 0.06f, 0.72f, 1.0f, 0.97f };
		case BossProjectileStyle::Abyss:
			return { 0.98f, 0.035f, 0.48f, 0.98f };
		case BossProjectileStyle::Jelly:
		default:
			return { 0.82f, 1.0f, 0.72f, 0.96f };
		}
	}

	inline DirectX::XMFLOAT4 GetBossProjectileGlowColor(BossProjectileStyle style)
	{
		switch (style)
		{
		case BossProjectileStyle::Knight:
			return { 1.0f, 0.035f, 0.01f, 0.38f };
		case BossProjectileStyle::MageViolet:
			return { 0.50f, 0.02f, 1.0f, 0.38f };
		case BossProjectileStyle::MageAzure:
			return { 0.01f, 0.48f, 1.0f, 0.38f };
		case BossProjectileStyle::Abyss:
			return { 0.88f, 0.01f, 0.40f, 0.40f };
		case BossProjectileStyle::Jelly:
		default:
			return { 0.34f, 1.0f, 0.10f, 0.32f };
		}
	}

	inline DirectX::XMFLOAT3 GetBossProjectileLightColor(BossProjectileStyle style)
	{
		const DirectX::XMFLOAT4 color = GetBossProjectileCoreColor(style);
		return { color.x, color.y, color.z };
	}

	inline MonsterType SelectMonsterType()
	{
		const int round = ProceduralMap_GetRound();
		float total_weight = 0.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			if (GetMapData().IsMonsterAllowed(round, type))
			{
				total_weight += GetMonsterData(type).SpawnWeight;
			}
		}
		if (total_weight <= 0.0f)
		{
			return MonsterType::Slime;
		}
		const float selection = RandomFloat(0.0f, total_weight);
		float accumulated = 0.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			if (!GetMapData().IsMonsterAllowed(round, type))
			{
				continue;
			}
			accumulated += GetMonsterData(type).SpawnWeight;
			if (selection < accumulated)
			{
				return type;
			}
		}
		return MonsterType::Slime;
	}

	inline int GetMonsterTextureID(MonsterType type)
	{
		const std::size_t index = MonsterTypeToIndex(type);
		return g_EnemySystem.MonsterTextureIDs[index < MONSTER_TYPE_COUNT ? index : 0u];
	}

	inline MonsterMapCollisionShape GetMonsterMapCollisionShape(MonsterType type, float draw_scale = 1.0f)
	{
		// 스켈레톤과 오크는 프레임 아래쪽에 몸이 몰려 있어 보이는 부분 기준으로 잡는다.
		MonsterMapCollisionShape shape{};
		switch (type)
		{
		case MonsterType::SkeletonBase:
		case MonsterType::SkeletonWarrior:
		case MonsterType::Orc:
		case MonsterType::OrcRogue:
			shape = { { 0.0f, 48.0f }, 48.0f };
			break;
		case MonsterType::SkeletonMage:
			shape = { { 0.0f, 46.5f }, 49.5f };
			break;
		case MonsterType::SkeletonRogue:
		case MonsterType::OrcWarrior:
			shape = { { 0.0f, 45.0f }, 51.0f };
			break;
		case MonsterType::OrcShaman:
			shape = { { 0.0f, 55.5f }, 42.0f };
			break;
		default:
			shape.Radius = GetMonsterData(type).CollisionRadius;
			break;
		}

		const float safe_scale = std::max(draw_scale, 0.01f);
		shape.Offset.x *= safe_scale;
		shape.Offset.y *= safe_scale;
		shape.Radius *= safe_scale;
		return shape;
	}

	inline DirectX::XMFLOAT2 GetEnemyAimPosition(const EnemySlot& slot)
	{
		const MonsterData& data = GetMonsterData(slot.Type);
		const bool flip_horizontal = slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
		const DirectX::XMFLOAT2 position = slot.Entity.GetPosition();
		return {
			position.x + (flip_horizontal ? -data.AimOffset.x : data.AimOffset.x) * slot.DrawScale,
			position.y + data.AimOffset.y * slot.DrawScale,
		};
	}

	inline DirectX::XMFLOAT2 GetBossFireVisualScale(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
		    slot.BossFireScaleElapsed >= EnemyConstants::BossJelly::FireScaleDuration)
		{
			return { 1.0f, 1.0f };
		}

		const float elapsed = std::max(slot.BossFireScaleElapsed, 0.0f);

		if (elapsed < 0.07f)
		{
			const float t = SmoothStep(elapsed / 0.07f);
			return { std::lerp(0.90f, 1.16f, t), std::lerp(0.82f, 1.20f, t) };
		}
		if (elapsed < 0.15f)
		{
			const float t = SmoothStep((elapsed - 0.07f) / 0.08f);
			return { std::lerp(1.16f, 0.97f, t), std::lerp(1.20f, 0.94f, t) };
		}
		const float t = SmoothStep((elapsed - 0.15f) / (EnemyConstants::BossJelly::FireScaleDuration - 0.15f));
		return { std::lerp(0.97f, 1.0f, t), std::lerp(0.94f, 1.0f, t) };
	}

	inline DirectX::XMFLOAT2 GetBossSplitVisualScale(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
		    slot.BossSplitScaleElapsed >= EnemyConstants::BossBody::SplitScaleDuration)
		{
			return { 1.0f, 1.0f };
		}

		const float elapsed = std::max(slot.BossSplitScaleElapsed, 0.0f);

		if (elapsed < 0.10f)
		{
			const float t = SmoothStep(elapsed / 0.10f);
			return { std::lerp(0.52f, 1.20f, t), std::lerp(0.40f, 1.16f, t) };
		}
		if (elapsed < 0.22f)
		{
			const float t = SmoothStep((elapsed - 0.10f) / 0.12f);
			return { std::lerp(1.20f, 0.95f, t), std::lerp(1.16f, 0.93f, t) };
		}
		const float t = SmoothStep((elapsed - 0.22f) / (EnemyConstants::BossBody::SplitScaleDuration - 0.22f));
		return { std::lerp(0.95f, 1.0f, t), std::lerp(0.93f, 1.0f, t) };
	}

	inline float GetBossSplitFlashAmount(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
		    slot.BossSplitScaleElapsed >= EnemyConstants::BossBody::SplitScaleDuration)
		{
			return 0.0f;
		}
		const float progress = Saturate(slot.BossSplitScaleElapsed / EnemyConstants::BossBody::SplitScaleDuration);
		return (1.0f - progress) * (1.0f - progress);
	}

	inline bool IsBossEncounterRoom(int room_index)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		return room && room->IsBossRoom;
	}

	inline bool IsSlimeRushRoom(const ProceduralMapRoom& room)
	{
		if (ProceduralMap_GetRound() != EnemyConstants::SlimeRush::Round || !room.IsLargeRoom || room.IsBossRoom)
		{
			return false;
		}

		return room.Depth == 1;
	}

	inline EnemySlot* FindAliveBossSlot()
	{
		for (EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			if (slot.Type == MonsterType::BossSlime && slot.Entity.IsAlive())
			{
				return &slot;
			}
		}
		return nullptr;
	}

	inline EnemySlot* FindAliveCorruptedBossSlot()
	{
		for (EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			if (IsCorruptedBossType(slot.Type) && slot.Entity.IsAlive())
			{
				return &slot;
			}
		}
		return nullptr;
	}

	inline EnemySlot* FindBossVolleySlot()
	{
		std::array<EnemySlot*, 4> alive_bosses{};
		int boss_count = 0;
		for (EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			if (slot.Type == MonsterType::BossSlime && slot.Entity.IsAlive() &&
			    boss_count < static_cast<int>(alive_bosses.size()))
			{
				alive_bosses[boss_count++] = &slot;
			}
		}
		if (boss_count <= 0)
		{
			return nullptr;
		}
		EnemySlot* selected = alive_bosses[g_EnemySystem.BossVolleySequence % boss_count];
		++g_EnemySystem.BossVolleySequence;
		return selected;
	}

	inline int GetEnemySlotID(const EnemySlot* target_slot)
	{
		if (!target_slot)
		{
			return -1;
		}
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			if (&g_EnemySystem.EnemySlots[enemy_id] == target_slot)
			{
				return enemy_id;
			}
		}
		return -1;
	}

	inline int GetEnemyExperienceDrop(const EnemySlot& slot)
	{
		return std::max(0,
		                static_cast<int>(std::round(GetMonsterData(slot.Type).ExperienceDrop * slot.ExperienceScale)));
	}

	void ClearBossJellyBullets();
	void UpdateCorruptedBossPattern(float delta_time, const DirectX::XMFLOAT2& player_position);
	DirectX::XMFLOAT2 GetBossJellyMuzzlePosition(const EnemySlot& boss_slot, const DirectX::XMFLOAT2& direction);
	DirectX::XMFLOAT2 GetBossJellyPrimaryDirection(const DirectX::XMFLOAT2& aim_direction, bool phase_two,
	                                               int shot_index);
	void TrySplitBossBody(const DirectX::XMFLOAT2& player_position);
	void UpdateBossJellyPattern(float delta_time, const DirectX::XMFLOAT2& player_position);

	void GetMonsterFrameRegion(MonsterType type, float animation_elapsed, int& frame_x, int& frame_y);
	void PlayDamageReaction(MonsterType type, const DirectX::XMFLOAT2& position,
	                        const DirectX::XMFLOAT2& impact_direction, bool was_killed);
	void HandleEnemyDefeat(EnemySlot& slot, const DirectX::XMFLOAT2& effect_position);
	void ApplyDashSlashPulse(PendingDashSlashAttack& attack);

	int FindFreeEnemySlot();
	int GetFreeEnemySlotCount();
	void ClearPendingEnemySpawns(int room_index);
	void BuildEnemyCollisionGrid();
	void CheckNearestEnemyInCell(int cell_x, int cell_y, const DirectX::XMFLOAT2& origin, bool& found,
	                             float& best_distance_sq, DirectX::XMFLOAT2& best_position);
	void ResolveEnemyOverlaps();
	bool IsFarEnoughFromEnemies(const DirectX::XMFLOAT2& position, float collision_radius,
	                            const std::vector<SpawnPlacement>& new_positions);

	void AbandonOtherActiveRooms(int entered_room_index);
	void BeginRoomEncounter(int room_index, const DirectX::XMFLOAT2& player_position);
	void UpdateRoomEncounter(int room_index, float delta_time, const DirectX::XMFLOAT2& player_position);

	bool TryGetEnemyAndProjectile(const cCollisionHit& hit, int& enemy_id, int& projectile_id);
	void ApplyProjectileDamage(int enemy_id, float damage, const DirectX::XMFLOAT2& knockback_direction,
	                           float knockback_speed);
	void ApplyAreaProjectileDamage(const DirectX::XMFLOAT2& center, float radius, float damage,
	                               const DirectX::XMFLOAT2& fallback_knockback_direction);

} // namespace GameEnemy::Internal

#endif // GAME_ENEMY_INTERNAL_H
