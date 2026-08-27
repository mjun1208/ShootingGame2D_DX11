#include "game_enemy.h"

#include "Audio.h"
#include "blood.h"
#include "chain_lightning.h"
#include "collision.h"
#include "enemy.h"
#include "enemy_attack_pattern.h"
#include "game_data_manager.h"
#include "game_damage_text.h"
#include "game_bullet.h"
#include "game_effect.h"
#include "game_experience_gem.h"
#include "game_healing_item.h"
#include "game_player.h"
#include "procedural_map.h"
#include "projectile.h"
#include "slime_goo.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
	const MonsterData& GetMonsterData(MonsterType type)
	{
		return GameDataManager::GetInstance().GetMonsterGameData().Get(type);
	}

	const MapGameData& GetMapData()
	{
		return GameDataManager::GetInstance().GetMapGameData();
	}

	constexpr int ENEMY_MAX = 512;
	constexpr float ENEMY_SPEED_VARIATION_RATIO = 0.18f;
	constexpr float PLAYER_BULLET_KNOCKBACK_SPEED = 100.0f;
	constexpr int DASH_SLASH_HIT_COUNT = 5;
	constexpr float DASH_SLASH_HIT_INTERVAL = 0.055f;
	constexpr const char* DASH_SLASH_HIT_SOUND_PATH =
		"asset/sound/leohpaz-22-slash-04.wav";
	constexpr const char* SLIME_DAMAGE_SOUND_PATH =
		"asset/sound/slime-02.wav";
	constexpr const char* SLIME_DEATH_SOUND_PATH =
		"asset/sound/slime-05.wav";
	constexpr const char* BOSS_SLIME_PROJECTILE_FIRE_SOUND_PATH =
		"asset/sound/slime-07.wav";
	constexpr const char* BAT_DAMAGE_SOUND_PATH =
		"asset/sound/freesound-468442-bat-damage.wav";
	constexpr const char* BAT_DEATH_SOUND_PATH =
		"asset/sound/freesound-445958-bat-death.wav";
	constexpr const char* BAT_DASH_SOUND_PATH =
		"asset/sound/pixabay-fast-swoosh-03-229316.wav";
	constexpr const char* BONES_DAMAGE_SOUND_PATH =
		"asset/sound/bones-rattle-2-damage.wav";
	constexpr const char* BONES_DEATH_SOUND_PATH =
		"asset/sound/bones-rattle-0-death.wav";
	constexpr const char* SKELETON_MAGE_FIRE_SOUND_PATH =
		"asset/sound/mixkit-wizard-fire-woosh-1326.wav";
	constexpr const char* ENEMY_WARRIOR_SLASH_SOUND_PATH =
		"asset/sound/leohpaz-22-slash-04.wav";
	constexpr std::array<const char*, 3> ORC_DAMAGE_SOUND_PATHS{
		"asset/sound/leohpaz-21-orc-damage-1.wav",
		"asset/sound/leohpaz-21-orc-damage-2.wav",
		"asset/sound/leohpaz-21-orc-damage-3.wav",
	};
	constexpr const char* ORC_DEATH_SOUND_PATH =
		"asset/sound/leohpaz-24-orc-death-spin.wav";
	constexpr const char* ORC_SHAMAN_CAST_SOUND_PATH =
		"asset/sound/mixkit-wizard-fire-woosh-1326.wav";
	constexpr float DASH_SLASH_KNOCKBACK_SPEED = 240.0f;
	constexpr float ENEMY_PLAYER_SPAWN_CLEARANCE = 330.0f;
	constexpr float ENEMY_WALL_PADDING = 8.0f;
	constexpr float ENEMY_SEPARATION_PADDING = 4.0f;
	constexpr int ENEMY_COLLISION_SOLVER_ITERATIONS = 4;
	constexpr int ENEMY_COLLISION_GRID_BUCKET_COUNT = 257;
	constexpr int ENEMY_COLLISION_GRID_INVALID_INDEX = -1;
	constexpr float ENCOUNTER_ENTRY_INSET = 72.0f;
	constexpr float BETWEEN_WAVE_DELAY = 0.65f;
	constexpr float SPAWN_RETRY_DELAY = 0.15f;
	constexpr float SPAWN_TELEGRAPH_DURATION = 0.5f;
	constexpr float SPAWN_TELEGRAPH_SIZE = 92.0f;
	constexpr float SPAWN_ARRIVAL_RING_DURATION = 0.24f;
	constexpr float SPAWN_ARRIVAL_RING_EXPANSION = 34.0f;
	constexpr int MAX_SPAWN_RETRIES = 20;
	constexpr int ORC_WARRIOR_DASH_AFTERIMAGE_PATH_CAPACITY = 2;
	constexpr int ORC_WARRIOR_DASH_AFTERIMAGE_COUNT = 6;
	// Dense boss spell patterns share this pool.  The old 256 limit silently
	// dropped the later waves of multi-ring patterns while earlier bullets lived.
	constexpr int BOSS_JELLY_BULLET_MAX = 768;
	constexpr float CORRUPTION_AURA_FRAME_WIDTH = 384.0f;
	constexpr float CORRUPTION_AURA_FRAME_HEIGHT = 512.0f;
	constexpr int CORRUPTION_AURA_COLUMN_COUNT = 4;
	constexpr int CORRUPTION_AURA_FRAME_COUNT = 8;
	constexpr float CORRUPTION_AURA_FRAME_DURATION = 0.10f;
	constexpr float BOSS_JELLY_FIRE_INTERVAL = 2.55f;
	constexpr float BOSS_JELLY_PHASE_TWO_FIRE_INTERVAL = 2.05f;
	constexpr float BOSS_JELLY_PRIMARY_SPEED = 360.0f;
	constexpr float BOSS_JELLY_PRIMARY_DISTANCE = 420.0f;
	constexpr float BOSS_JELLY_SPLIT_SPEED = 285.0f;
	constexpr float BOSS_JELLY_SPLIT_DISTANCE = 680.0f;
	constexpr float BOSS_JELLY_PRIMARY_DAMAGE = 15.0f;
	constexpr float BOSS_JELLY_SPLIT_DAMAGE = 8.0f;
	// Phase two fires two primary jelly shots around the locked aim direction.
	constexpr float BOSS_JELLY_PAIR_ANGLE_OFFSET = 0.18f;
	constexpr float BOSS_JELLY_TELEGRAPH_DURATION = 0.65f;
	constexpr float BOSS_JELLY_TELEGRAPH_TRACE_STEP = 10.0f;
	constexpr float BOSS_JELLY_FIRE_SCALE_DURATION = 0.28f;
	constexpr float BOSS_BODY_SPLIT_SCALE_DURATION = 0.36f;
	constexpr float BOSS_BODY_SPLIT_SCALE = 0.68f;
	constexpr float BOSS_BODY_SPLIT_OFFSET = 118.0f;
	constexpr float BOSS_BODY_SPLIT_SPEED_SCALE = 1.35f;
	constexpr float BOSS_BODY_SECOND_SPLIT_SCALE = 0.72f;
	constexpr float BOSS_BODY_SECOND_SPLIT_OFFSET = 86.0f;
	constexpr float BOSS_BODY_SECOND_SPLIT_SPEED_SCALE = 1.70f;
	constexpr MonsterType DEFAULT_MONSTER_TYPE = MonsterType::SkeletonBase;
	constexpr std::array<MonsterType, 14> MONSTER_TYPES{
		MonsterType::Slime,
		MonsterType::SkeletonBase,
		MonsterType::SkeletonMage,
		MonsterType::SkeletonRogue,
		MonsterType::SkeletonWarrior,
		MonsterType::Bat,
		MonsterType::Orc,
		MonsterType::OrcRogue,
		MonsterType::OrcShaman,
		MonsterType::OrcWarrior,
		MonsterType::BossSlime,
		MonsterType::BossCorruptedKnight,
		MonsterType::BossCorruptedMage,
		MonsterType::BossCthulhu,
	};
	constexpr std::size_t MONSTER_TYPE_COUNT = MONSTER_TYPES.size();

	bool IsBossType(MonsterType type)
	{
		return type == MonsterType::BossSlime ||
			type == MonsterType::BossCorruptedKnight ||
			type == MonsterType::BossCorruptedMage ||
			type == MonsterType::BossCthulhu;
	}

	bool IsCorruptedBossType(MonsterType type)
	{
		return type == MonsterType::BossCorruptedKnight ||
			type == MonsterType::BossCorruptedMage ||
			type == MonsterType::BossCthulhu;
	}

	bool IsOrcType(MonsterType type)
	{
		return type == MonsterType::Orc ||
			type == MonsterType::OrcRogue ||
			type == MonsterType::OrcShaman ||
			type == MonsterType::OrcWarrior;
	}

	MonsterType GetBossTypeForRound(int round)
	{
		switch (round)
		{
		case 1: return MonsterType::BossSlime;
		case 2: return MonsterType::BossCorruptedKnight;
		case 3: return MonsterType::BossCorruptedMage;
		default: return MonsterType::BossCthulhu;
		}
	}

	float GetSpawnTelegraphSize(MonsterType type)
	{
		if (!IsBossType(type))
		{
			return SPAWN_TELEGRAPH_SIZE;
		}

		const DirectX::XMFLOAT2 boss_draw_size = GetMonsterData(type).DrawSize;
		return std::max(
			SPAWN_TELEGRAPH_SIZE,
			std::min(boss_draw_size.x, boss_draw_size.y));
	}

	constexpr int BONE_DROP_FRAME_COUNT = 3;
	constexpr int BONE_DROP_FRAME_WIDTH = 16;
	constexpr int BONE_DROP_FRAME_HEIGHT = 16;
	constexpr int BONE_BURST_PARTICLE_COUNT = 8;
	constexpr float BONE_BURST_GRAVITY = 280.0f;
	constexpr float BONE_BURST_DRAG_PER_SECOND = 0.18f;
	constexpr float BONE_DROP_TEXTURE_WIDTH = 48.0f;
	constexpr float BONE_DROP_TEXTURE_HEIGHT = 16.0f;

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
		int SpawnRetryCount{ 0 };
		float DelayRemaining{ 0.0f };
	};

	struct EnemySlot
	{
		cEnemy Entity{};
		MonsterType Type{ DEFAULT_MONSTER_TYPE };
		int RoomIndex{ -1 };
		float DrawScale{ 1.0f };
		float ExperienceScale{ 1.0f };
		float BossFireScaleElapsed{ BOSS_JELLY_FIRE_SCALE_DURATION };
		float BossSplitScaleElapsed{ BOSS_BODY_SPLIT_SCALE_DURATION };
	};

	void ApplyCombatKnockback(
		EnemySlot& slot,
		const DirectX::XMFLOAT2& direction,
		float speed)
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
		MonsterType Type{ DEFAULT_MONSTER_TYPE };
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
		float BaseSize{ SPAWN_TELEGRAPH_SIZE };
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
		bool IsPrimary{ false };
		BossProjectileStyle Style{ BossProjectileStyle::Jelly };
		bool IsActive{ false };
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
		int BucketHeads[ENEMY_COLLISION_GRID_BUCKET_COUNT]{};
		int NextEnemy[ENEMY_MAX]{};
		int CellX[ENEMY_MAX]{};
		int CellY[ENEMY_MAX]{};
		int MinCellX{};
		int MaxCellX{};
		int MinCellY{};
		int MaxCellY{};
		bool HasAliveEnemy{ false };
	};

	EnemySlot g_EnemySlots[ENEMY_MAX];
	EnemyCollisionGrid g_EnemyCollisionGrid;
	std::vector<RoomWaveRuntime> g_RoomWaves;
	std::vector<bool> g_DiscoveredRooms;
	std::vector<PendingEnemySpawn> g_PendingEnemySpawns;
	std::vector<SpawnArrivalEffect> g_SpawnArrivalEffects;
	std::vector<BoneDropEffect> g_BoneDropEffects;
	std::vector<PendingDashSlashAttack> g_PendingDashSlashAttacks;
	std::array<int, DASH_SLASH_HIT_COUNT> g_DashSlashHitAudioIDs = []
	{
		std::array<int, DASH_SLASH_HIT_COUNT> audio_ids{};
		audio_ids.fill(-1);
		return audio_ids;
	}();
	int g_SlimeDamageAudioID = -1;
	int g_SlimeDeathAudioID = -1;
	int g_BossSlimeProjectileFireAudioID = -1;
	int g_BatDamageAudioID = -1;
	int g_BatDeathAudioID = -1;
	int g_BatDashAudioID = -1;
	int g_BonesDamageAudioID = -1;
	int g_BonesDeathAudioID = -1;
	std::array<int, 3> g_SkeletonMageFireAudioIDs{ -1, -1, -1 };
	int g_EnemyWarriorSlashAudioID = -1;
	std::array<int, ORC_DAMAGE_SOUND_PATHS.size()> g_OrcDamageAudioIDs = []
	{
		std::array<int, ORC_DAMAGE_SOUND_PATHS.size()> audio_ids{};
		audio_ids.fill(-1);
		return audio_ids;
	}();
	int g_OrcDeathAudioID = -1;
	int g_OrcShamanCastAudioID = -1;
	std::array<int, MONSTER_TYPE_COUNT> g_MonsterTextureIDs = []
	{
		std::array<int, MONSTER_TYPE_COUNT> texture_ids{};
		texture_ids.fill(TEXTURE_INVALID_ID);
		return texture_ids;
	}();
	int g_BoneDropTextureID = TEXTURE_INVALID_ID;
	int g_DaggerTextureID = TEXTURE_INVALID_ID;
	int g_AxeTextureID = TEXTURE_INVALID_ID;
	int g_MageProjectileTextureID = TEXTURE_INVALID_ID;
	int g_SpawnTelegraphTextureID = TEXTURE_INVALID_ID;
	int g_BossJellyTextureID = TEXTURE_INVALID_ID;
	int g_BossJellyTelegraphTextureID = TEXTURE_INVALID_ID;
	int g_CorruptionProjectileTextureID = TEXTURE_INVALID_ID;
	int g_CorruptionAuraTextureID = TEXTURE_INVALID_ID;
	std::array<BossJellyBullet, BOSS_JELLY_BULLET_MAX> g_BossJellyBullets{};
	float g_BossJellyFireCooldown = BOSS_JELLY_FIRE_INTERVAL;
	int g_BossJellyTelegraphEnemyID = -1;
	DirectX::XMFLOAT2 g_BossJellyTelegraphDirection{ 1.0f, 0.0f };
	int g_BossSplitStage = 0;
	int g_BossVolleySequence = 0;
	float g_CorruptionFireCooldown = 0.8f;
	int g_CorruptionVolleySequence = 0;
	std::array<DirectX::XMUINT2, MONSTER_TYPE_COUNT> g_MonsterTextureSizes{};
	int g_CurrentRoomIndex = -1;
	bool g_RoundCleared = false;
	float g_MonsterAnimationElapsed = 0.0f;
	float g_EnemyCollisionCellSize = cEnemy::RADIUS * 2.0f;
	std::uint32_t g_BoneRandomState = 0xB04E5EEDu;
	std::uint32_t g_OrcDamageSoundRandomState = 0x0ACD4A6Eu;

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
		return enemy_id >= 0 && enemy_id < ENEMY_MAX;
	}

	std::size_t MonsterTypeToIndex(MonsterType type)
	{
		const int index = static_cast<int>(type) - 1;
		return index >= 0 && index < static_cast<int>(MONSTER_TYPE_COUNT) ?
			static_cast<std::size_t>(index) : 0u;
	}

	MonsterLightStyle GetMonsterLightStyle(MonsterType type)
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

	DirectX::XMFLOAT4 GetBossProjectileCoreColor(BossProjectileStyle style)
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

	DirectX::XMFLOAT4 GetBossProjectileGlowColor(BossProjectileStyle style)
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

	DirectX::XMFLOAT3 GetBossProjectileLightColor(BossProjectileStyle style)
	{
		const DirectX::XMFLOAT4 color = GetBossProjectileCoreColor(style);
		return { color.x, color.y, color.z };
	}

	MonsterType SelectMonsterType(std::uint32_t hash)
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
		if (total_weight <= 0.0f) return MonsterType::Slime;
		const float selection =
			static_cast<float>(hash & 0x00ffffffu) /
			static_cast<float>(0x01000000u) * total_weight;
		float accumulated = 0.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			if (!GetMapData().IsMonsterAllowed(round, type)) continue;
			accumulated += GetMonsterData(type).SpawnWeight;
			if (selection < accumulated) return type;
		}
		return MonsterType::Slime;
	}

	int GetMonsterTextureID(MonsterType type)
	{
		return g_MonsterTextureIDs[MonsterTypeToIndex(type)];
	}

	MonsterMapCollisionShape GetMonsterMapCollisionShape(
		MonsterType type,
		float draw_scale = 1.0f)
	{
		// The 64x64 skeleton/orc frames are bottom-aligned and drawn at 3x.
		// These shapes enclose their opaque animation bounds so the visible body,
		// rather than the mostly empty frame center, keeps the wall padding.
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

	DirectX::XMFLOAT2 GetEnemyAimPosition(const EnemySlot& slot)
	{
		const MonsterData& data = GetMonsterData(slot.Type);
		const bool flip_horizontal =
			slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
		const DirectX::XMFLOAT2 position = slot.Entity.GetPosition();
		return {
			position.x + (flip_horizontal ? -data.AimOffset.x : data.AimOffset.x) *
				slot.DrawScale,
			position.y + data.AimOffset.y * slot.DrawScale,
		};
	}

	DirectX::XMFLOAT2 GetBossFireVisualScale(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
			slot.BossFireScaleElapsed >= BOSS_JELLY_FIRE_SCALE_DURATION)
		{
			return { 1.0f, 1.0f };
		}

		const float elapsed = std::max(slot.BossFireScaleElapsed, 0.0f);
		auto smooth_step = [](float amount)
		{
			const float t = std::clamp(amount, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		};
		auto lerp = [](float start, float end, float amount)
		{
			return start + (end - start) * amount;
		};

		if (elapsed < 0.07f)
		{
			const float t = smooth_step(elapsed / 0.07f);
			return { lerp(0.90f, 1.16f, t), lerp(0.82f, 1.20f, t) };
		}
		if (elapsed < 0.15f)
		{
			const float t = smooth_step((elapsed - 0.07f) / 0.08f);
			return { lerp(1.16f, 0.97f, t), lerp(1.20f, 0.94f, t) };
		}
		const float t = smooth_step(
			(elapsed - 0.15f) / (BOSS_JELLY_FIRE_SCALE_DURATION - 0.15f));
		return { lerp(0.97f, 1.0f, t), lerp(0.94f, 1.0f, t) };
	}

	DirectX::XMFLOAT2 GetBossSplitVisualScale(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
			slot.BossSplitScaleElapsed >= BOSS_BODY_SPLIT_SCALE_DURATION)
		{
			return { 1.0f, 1.0f };
		}

		const float elapsed = std::max(slot.BossSplitScaleElapsed, 0.0f);
		auto smooth_step = [](float amount)
		{
			const float t = std::clamp(amount, 0.0f, 1.0f);
			return t * t * (3.0f - 2.0f * t);
		};
		auto lerp = [](float start, float end, float amount)
		{
			return start + (end - start) * amount;
		};

		if (elapsed < 0.10f)
		{
			const float t = smooth_step(elapsed / 0.10f);
			return { lerp(0.52f, 1.20f, t), lerp(0.40f, 1.16f, t) };
		}
		if (elapsed < 0.22f)
		{
			const float t = smooth_step((elapsed - 0.10f) / 0.12f);
			return { lerp(1.20f, 0.95f, t), lerp(1.16f, 0.93f, t) };
		}
		const float t = smooth_step(
			(elapsed - 0.22f) / (BOSS_BODY_SPLIT_SCALE_DURATION - 0.22f));
		return { lerp(0.95f, 1.0f, t), lerp(0.93f, 1.0f, t) };
	}

	float GetBossSplitFlashAmount(const EnemySlot& slot)
	{
		if (slot.Type != MonsterType::BossSlime ||
			slot.BossSplitScaleElapsed >= BOSS_BODY_SPLIT_SCALE_DURATION)
		{
			return 0.0f;
		}
		const float progress = std::clamp(
			slot.BossSplitScaleElapsed / BOSS_BODY_SPLIT_SCALE_DURATION,
			0.0f, 1.0f);
		return (1.0f - progress) * (1.0f - progress);
	}

	bool IsBossEncounterRoom(int room_index)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		return room && room->IsBossRoom;
	}

	EnemySlot* FindAliveBossSlot()
	{
		for (EnemySlot& slot : g_EnemySlots)
		{
			if (slot.Type == MonsterType::BossSlime && slot.Entity.IsAlive())
			{
				return &slot;
			}
		}
		return nullptr;
	}

	EnemySlot* FindAliveCorruptedBossSlot()
	{
		for (EnemySlot& slot : g_EnemySlots)
		{
			if (IsCorruptedBossType(slot.Type) && slot.Entity.IsAlive())
			{
				return &slot;
			}
		}
		return nullptr;
	}

	EnemySlot* FindBossVolleySlot()
	{
		std::array<EnemySlot*, 4> alive_bosses{};
		int boss_count = 0;
		for (EnemySlot& slot : g_EnemySlots)
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
		EnemySlot* selected = alive_bosses[g_BossVolleySequence % boss_count];
		++g_BossVolleySequence;
		return selected;
	}

	int GetEnemySlotID(const EnemySlot* target_slot)
	{
		if (!target_slot)
		{
			return -1;
		}
		for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
		{
			if (&g_EnemySlots[enemy_id] == target_slot)
			{
				return enemy_id;
			}
		}
		return -1;
	}

	int GetEnemyExperienceDrop(const EnemySlot& slot)
	{
		return std::max(
			0,
			static_cast<int>(std::round(
				GetMonsterData(slot.Type).ExperienceDrop * slot.ExperienceScale)));
	}

	void ClearBossJellyBullets()
	{
		for (BossJellyBullet& bullet : g_BossJellyBullets)
		{
			bullet = BossJellyBullet{};
		}
	}

	void ClearCorruptionBullets()
	{
		for (BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (bullet.Style != BossProjectileStyle::Jelly)
			{
				bullet.IsActive = false;
			}
		}
	}

	bool SpawnBossJellyBullet(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		bool is_primary,
		float speed_scale = 1.0f,
		float size_scale = 1.0f)
	{
		for (BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (bullet.IsActive)
			{
				continue;
			}

			const float speed = (is_primary ?
				BOSS_JELLY_PRIMARY_SPEED : BOSS_JELLY_SPLIT_SPEED) * speed_scale;
			bullet = BossJellyBullet{};
			bullet.Position = position;
			bullet.Velocity = { direction.x * speed, direction.y * speed };
			bullet.Radius = (is_primary ? 30.0f : 18.0f) * size_scale;
			bullet.Size = (is_primary ? 84.0f : 52.0f) * size_scale;
			bullet.Damage = is_primary ?
				BOSS_JELLY_PRIMARY_DAMAGE : BOSS_JELLY_SPLIT_DAMAGE;
			bullet.MaxDistance = is_primary ?
				BOSS_JELLY_PRIMARY_DISTANCE : BOSS_JELLY_SPLIT_DISTANCE;
			bullet.Rotation = std::atan2(
				bullet.Velocity.x, -bullet.Velocity.y);
			bullet.IsPrimary = is_primary;
			bullet.Style = BossProjectileStyle::Jelly;
			bullet.IsActive = true;
			return true;
		}
		return false;
	}

	bool SpawnCorruptionBullet(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		float speed,
		float size,
		float damage,
		float maximum_distance,
		BossProjectileStyle style,
		float turn_rate = 0.0f,
		float acceleration = 0.0f)
	{
		for (BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (bullet.IsActive)
			{
				continue;
			}

			bullet = BossJellyBullet{};
			bullet.Position = position;
			bullet.Velocity = { direction.x * speed, direction.y * speed };
			// The bright sprite is deliberately larger than its damaging core,
			// allowing close but fair grazing through dense patterns.
			bullet.Radius = size * 0.22f;
			bullet.Size = size;
			bullet.Damage = damage;
			bullet.MaxDistance = maximum_distance;
			bullet.Rotation = std::atan2(direction.y, direction.x);
			bullet.TurnRate = turn_rate;
			bullet.Acceleration = acceleration;
			bullet.Style = style;
			bullet.IsActive = true;
			return true;
		}
		return false;
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

	DirectX::XMFLOAT2 GetAimDirection(
		const DirectX::XMFLOAT2& origin,
		const DirectX::XMFLOAT2& target)
	{
		const float dx = target.x - origin.x;
		const float dy = target.y - origin.y;
		const float distance_squared = dx * dx + dy * dy;
		if (distance_squared <= 0.0001f)
		{
			return { 1.0f, 0.0f };
		}
		const float inverse_distance = 1.0f / std::sqrt(distance_squared);
		return { dx * inverse_distance, dy * inverse_distance };
	}

	void FireCorruptionFan(
		const EnemySlot& boss_slot,
		const DirectX::XMFLOAT2& player_position,
		int shot_count,
		float angle_step,
		float speed,
		float size,
		float damage,
		BossProjectileStyle style,
		float turn_rate = 0.0f)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const DirectX::XMFLOAT2 aim_direction =
			GetAimDirection(origin, player_position);
		const float center = static_cast<float>(shot_count - 1) * 0.5f;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const DirectX::XMFLOAT2 direction = RotateDirection(
				aim_direction,
				(static_cast<float>(shot_index) - center) * angle_step);
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
			};
			SpawnCorruptionBullet(
				muzzle, direction, speed, size, damage, 1100.0f,
				style, turn_rate);
		}
	}

	void FireCorruptionRing(
		const EnemySlot& boss_slot,
		int shot_count,
		float angle_offset,
		float speed,
		float size,
		float damage,
		BossProjectileStyle style,
		float turn_rate = 0.0f,
		float gap_center_angle = 0.0f,
		float gap_half_angle = 0.0f)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const float angle = angle_offset + DirectX::XM_2PI *
				static_cast<float>(shot_index) / static_cast<float>(shot_count);
			if (gap_half_angle > 0.0f)
			{
				const float gap_delta = std::atan2(
					std::sin(angle - gap_center_angle),
					std::cos(angle - gap_center_angle));
				if (std::abs(gap_delta) < gap_half_angle)
				{
					continue;
				}
			}
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 10.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 10.0f),
			};
			SpawnCorruptionBullet(
				muzzle, direction, speed, size, damage, 1050.0f,
				style, turn_rate);
		}
	}

	void FireCthulhuCrossfire(
		const DirectX::XMFLOAT2& player_position,
		int sequence,
		bool phase_two)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(
			ProceduralMap_GetRoomIndexAt(player_position));
		if (!room)
		{
			return;
		}
		const int lane_count = phase_two ? 7 : 5;
		const int safe_lane = lane_count / 2;
		const float center = static_cast<float>(lane_count - 1) * 0.5f;
		const float spacing = phase_two ? 72.0f : 84.0f;
		const float left_x = room->WorldMin.x + 82.0f;
		const float right_x = room->WorldMax.x - 82.0f;
		const float maximum_distance =
			(room->WorldMax.x - room->WorldMin.x) + 120.0f;
		for (int lane = 0; lane < lane_count; ++lane)
		{
			if (lane == safe_lane)
			{
				continue;
			}
			const float y = player_position.y +
				(static_cast<float>(lane) - center) * spacing;
			if (y <= room->WorldMin.y + 70.0f ||
				y >= room->WorldMax.y - 70.0f)
			{
				continue;
			}
			const bool from_left = ((lane + sequence) & 1) == 0;
			SpawnCorruptionBullet(
				{ from_left ? left_x : right_x, y },
				{ from_left ? 1.0f : -1.0f, 0.0f },
				phase_two ? 335.0f : 295.0f,
				30.0f, 9.0f, maximum_distance,
				from_left ? BossProjectileStyle::MageAzure :
					BossProjectileStyle::Abyss);
		}
	}

	void FireCthulhuAcceleratingBurst(
		const EnemySlot& boss_slot,
		const DirectX::XMFLOAT2& player_position,
		int sequence,
		bool phase_two,
		bool final_phase)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const DirectX::XMFLOAT2 aim = GetAimDirection(origin, player_position);
		const float aim_angle = std::atan2(aim.y, aim.x);
		const int shot_count = final_phase ? 12 : (phase_two ? 10 : 8);
		const float angle_offset = aim_angle +
			static_cast<float>(sequence) * 0.17f;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const float angle = angle_offset + DirectX::XM_2PI *
				static_cast<float>(shot_index) / static_cast<float>(shot_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x *
					(boss_slot.Entity.GetCollisionRadius() + 10.0f),
				origin.y + direction.y *
					(boss_slot.Entity.GetCollisionRadius() + 10.0f),
			};
			SpawnCorruptionBullet(
				muzzle, direction,
				final_phase ? 165.0f : 135.0f,
				31.0f, 9.0f, 1050.0f,
				(shot_index & 1) == 0 ? BossProjectileStyle::Abyss :
					BossProjectileStyle::MageViolet,
				0.0f,
				final_phase ? 145.0f : 112.0f);
		}
	}

	void UpdateCorruptedBossPattern(
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		EnemySlot* boss_slot = FindAliveCorruptedBossSlot();
		if (!boss_slot)
		{
			g_CorruptionFireCooldown = 0.8f;
			g_CorruptionVolleySequence = 0;
			return;
		}
		if (boss_slot->Type == MonsterType::BossCthulhu &&
			!EnemyAttackPattern::IsBossAttackWindow(
				GetEnemySlotID(boss_slot)))
		{
			// Hold the spell until the matching attack animation begins.  This
			// also creates a clean dodge/rest window around the cross-room dash.
			ClearCorruptionBullets();
			g_CorruptionFireCooldown = 0.0f;
			return;
		}

		g_CorruptionFireCooldown -= delta_time;
		if (g_CorruptionFireCooldown > 0.0f)
		{
			return;
		}

		const float health_ratio = boss_slot->Entity.GetHitPointRatio();
		const bool phase_two = health_ratio <= 0.55f;
		const bool final_phase = health_ratio <= 0.25f;
		const int sequence = g_CorruptionVolleySequence;
		switch (boss_slot->Type)
		{
		case MonsterType::BossCorruptedKnight:
		{
			// Scarlet sword sign: three quick aimed cuts, then a rotating wall
			// with a clearly readable lane aimed at the player.
			const int cycle_length = phase_two ? 5 : 4;
			const int cycle_step = sequence % cycle_length;
			if (cycle_step < 3)
			{
				const float turn = (cycle_step & 1) == 0 ? 0.045f : -0.045f;
				FireCorruptionFan(
					*boss_slot, player_position,
					final_phase ? 7 : (phase_two ? 5 : 3),
					0.125f, final_phase ? 520.0f : 480.0f,
					36.0f, 10.0f, BossProjectileStyle::Knight, turn);
				g_CorruptionFireCooldown = final_phase ? 0.24f : 0.31f;
			}
			else
			{
				const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(*boss_slot);
				const DirectX::XMFLOAT2 aim = GetAimDirection(origin, player_position);
				const float aim_angle = std::atan2(aim.y, aim.x);
				const float turn = (sequence & 1) == 0 ? 0.085f : -0.085f;
				FireCorruptionRing(
					*boss_slot, final_phase ? 18 : (phase_two ? 16 : 14),
					aim_angle + 0.11f, phase_two ? 305.0f : 275.0f,
					34.0f, 8.0f, BossProjectileStyle::Knight, turn,
					aim_angle, 0.40f);
				if (phase_two && cycle_step == 4)
				{
					FireCorruptionRing(
						*boss_slot, final_phase ? 8 : 6,
						aim_angle + DirectX::XM_PIDIV4,
					255.0f, 36.0f, 8.0f,
						BossProjectileStyle::Knight, -turn);
				}
				g_CorruptionFireCooldown = final_phase ? 0.92f : 1.18f;
			}
			break;
		}

		case MonsterType::BossCorruptedMage:
		{
			// Star magic sign: counter-rotating streams form a persistent
			// double spiral instead of isolated circular bursts.
			const int spoke_count = final_phase ? 5 : (phase_two ? 4 : 3);
			const float spiral_angle = static_cast<float>(sequence) *
				(final_phase ? 0.29f : 0.24f);
			FireCorruptionRing(
				*boss_slot, spoke_count, spiral_angle,
				final_phase ? 250.0f : 220.0f, 30.0f, 7.0f,
				BossProjectileStyle::MageViolet,
				final_phase ? 0.30f : 0.23f);
			FireCorruptionRing(
				*boss_slot, spoke_count,
				-spiral_angle + DirectX::XM_PI / static_cast<float>(spoke_count),
				final_phase ? 325.0f : 285.0f, 28.0f, 7.0f,
				BossProjectileStyle::MageAzure,
				final_phase ? -0.24f : -0.18f);
			if ((sequence % 5) == 4)
			{
				FireCorruptionFan(
					*boss_slot, player_position, phase_two ? 3 : 1, 0.18f,
					390.0f, 32.0f, 9.0f,
					BossProjectileStyle::MageAzure);
			}
			g_CorruptionFireCooldown = final_phase ? 0.20f :
				(phase_two ? 0.24f : 0.30f);
			if ((sequence % 10) == 9)
			{
				g_CorruptionFireCooldown += final_phase ? 0.90f : 1.25f;
			}
			break;
		}

		case MonsterType::BossCthulhu:
		{
			const int attack_variant =
				EnemyAttackPattern::GetBossAttackVariant(
					GetEnemySlotID(boss_slot));
			switch (attack_variant)
			{
			case 0:
				// Gungeon-style shotgun: short, readable cones that demand a
				// sidestep instead of filling the entire arena.
				FireCorruptionFan(
					*boss_slot, player_position,
					final_phase ? 7 : (phase_two ? 5 : 5), 0.16f,
					final_phase ? 470.0f : 415.0f, 33.0f, 10.0f,
					BossProjectileStyle::Abyss);
				g_CorruptionFireCooldown = final_phase ? 0.32f : 0.39f;
				break;

			case 1:
				// Alternating bullets enter from both arena walls, leaving the
				// player's current lane open as an immediately legible safe route.
				FireCthulhuCrossfire(
					player_position, sequence, phase_two);
				g_CorruptionFireCooldown = final_phase ? 0.50f : 0.62f;
				break;

			case 2:
			{
				// Curved crown with a generous wedge facing the player.
				const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(*boss_slot);
				const DirectX::XMFLOAT2 aim = GetAimDirection(origin, player_position);
				const float aim_angle = std::atan2(aim.y, aim.x);
				const float crown_angle = static_cast<float>(sequence) * 0.21f;
				FireCorruptionRing(
					*boss_slot, final_phase ? 16 : (phase_two ? 14 : 12),
					crown_angle, phase_two ? 270.0f : 235.0f,
					31.0f, 9.0f, BossProjectileStyle::MageViolet,
					(sequence & 1) == 0 ? 0.11f : -0.11f,
					aim_angle, 0.48f);
				if (phase_two)
				{
					FireCorruptionRing(
						*boss_slot, final_phase ? 10 : 8,
						-crown_angle + DirectX::XM_PIDIV4,
						320.0f, 26.0f, 8.0f,
						BossProjectileStyle::MageAzure, -0.08f,
						aim_angle, 0.55f);
				}
				g_CorruptionFireCooldown = final_phase ? 0.46f : 0.58f;
				break;
			}

			case 3:
			default:
				// Slow bullets visibly fan out first, then accelerate through the
				// gaps.  The delayed speed change gives a dodge-roll-like beat.
				FireCthulhuAcceleratingBurst(
					*boss_slot, player_position, sequence,
					phase_two, final_phase);
				g_CorruptionFireCooldown = final_phase ? 0.40f : 0.50f;
				break;
			}
			break;
		}

		default:
			g_CorruptionFireCooldown = 1.0f;
			break;
		}
		++g_CorruptionVolleySequence;
	}

	void SplitBossJellyBullet(const DirectX::XMFLOAT2& position)
	{
		// Bubble flower sign: the large glob opens into two offset rings.  Each
		// body split adds petals, so the silhouette escalates with the phase.
		const int outer_count = 8 + g_BossSplitStage * 2;
		const int inner_count = 4 + g_BossSplitStage;
		const float base_angle = static_cast<float>(g_BossVolleySequence) * 0.23f;
		for (int direction_index = 0; direction_index < outer_count; ++direction_index)
		{
			const float angle = base_angle + DirectX::XM_2PI *
				static_cast<float>(direction_index) / static_cast<float>(outer_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 spawn_position{
				position.x + direction.x * 12.0f,
				position.y + direction.y * 12.0f,
			};
			SpawnBossJellyBullet(spawn_position, direction, false, 1.08f, 0.86f);
		}
		for (int direction_index = 0; direction_index < inner_count; ++direction_index)
		{
			const float angle = base_angle + DirectX::XM_PI /
				static_cast<float>(inner_count) + DirectX::XM_2PI *
				static_cast<float>(direction_index) / static_cast<float>(inner_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 spawn_position{
				position.x + direction.x * 8.0f,
				position.y + direction.y * 8.0f,
			};
			SpawnBossJellyBullet(spawn_position, direction, false, 0.64f, 0.66f);
		}
		cSlimeGoo::GetInstance().Spawn(
			position,
			{ 0.0f, 0.0f },
			0.9f);
	}

	DirectX::XMFLOAT2 GetBossJellyMuzzlePosition(
		const EnemySlot& boss_slot,
		const DirectX::XMFLOAT2& direction)
	{
		const cEnemy& boss = boss_slot.Entity;
		const DirectX::XMFLOAT2 boss_position = GetEnemyAimPosition(boss_slot);
		const float muzzle_offset = boss.GetCollisionRadius() + 22.0f;
		return {
			boss_position.x + direction.x * muzzle_offset,
			boss_position.y + direction.y * muzzle_offset,
		};
	}

	DirectX::XMFLOAT2 TraceBossJellyPath(
		const DirectX::XMFLOAT2& start_position,
		const DirectX::XMFLOAT2& direction,
		float maximum_distance,
		float radius)
	{
		DirectX::XMFLOAT2 traced_position = start_position;
		float travelled = 0.0f;
		while (travelled < maximum_distance)
		{
			const float step = std::min(
				BOSS_JELLY_TELEGRAPH_TRACE_STEP,
				maximum_distance - travelled);
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

	DirectX::XMFLOAT2 GetBossJellyPrimaryDirection(
		const DirectX::XMFLOAT2& aim_direction,
		bool phase_two,
		int shot_index)
	{
		if (!phase_two)
		{
			return aim_direction;
		}
		const float angle_offset = shot_index == 0 ?
			-BOSS_JELLY_PAIR_ANGLE_OFFSET : BOSS_JELLY_PAIR_ANGLE_OFFSET;
		const float sine = std::sin(angle_offset);
		const float cosine = std::cos(angle_offset);
		return {
			aim_direction.x * cosine - aim_direction.y * sine,
			aim_direction.x * sine + aim_direction.y * cosine,
		};
	}

	void FireBossJellyBullet(
		EnemySlot& boss_slot,
		const DirectX::XMFLOAT2& aim_direction,
		bool phase_two)
	{
		boss_slot.BossFireScaleElapsed = 0.0f;
		const int shot_count = phase_two ? 2 : 1;
		bool fired = false;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const DirectX::XMFLOAT2 direction = GetBossJellyPrimaryDirection(
				aim_direction, phase_two, shot_index);
			const DirectX::XMFLOAT2 muzzle_position =
				GetBossJellyMuzzlePosition(boss_slot, direction);
			fired = SpawnBossJellyBullet(
				muzzle_position, direction, true) || fired;
		}
		if (fired && g_BossSlimeProjectileFireAudioID >= 0)
		{
			PlayAudio(g_BossSlimeProjectileFireAudioID);
		}
	}

	void BeginBossJellyTelegraph(const DirectX::XMFLOAT2& player_position)
	{
		EnemySlot* firing_boss = FindBossVolleySlot();
		if (!firing_boss)
		{
			g_BossJellyTelegraphEnemyID = -1;
			return;
		}

		const DirectX::XMFLOAT2 boss_position = GetEnemyAimPosition(*firing_boss);
		const float dx = player_position.x - boss_position.x;
		const float dy = player_position.y - boss_position.y;
		const float distance_squared = dx * dx + dy * dy;
		if (distance_squared <= 0.0001f)
		{
			g_BossJellyTelegraphEnemyID = -1;
			return;
		}

		const float inverse_distance = 1.0f / std::sqrt(distance_squared);
		g_BossJellyTelegraphEnemyID = GetEnemySlotID(firing_boss);
		g_BossJellyTelegraphDirection = {
			dx * inverse_distance,
			dy * inverse_distance,
		};
	}

	void TrySplitBossBody(const DirectX::XMFLOAT2& player_position)
	{
		if (g_BossSplitStage >= 2)
		{
			return;
		}

		const MonsterData& boss_data = GetMonsterData(MonsterType::BossSlime);
		std::array<int, 2> source_indices{ -1, -1 };
		std::array<int, 2> free_indices{ -1, -1 };
		int source_count = 0;
		float total_hit_point = 0.0f;
		for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
		{
			const EnemySlot& slot = g_EnemySlots[enemy_id];
			if (slot.Type == MonsterType::BossSlime && slot.Entity.IsAlive() &&
				source_count < static_cast<int>(source_indices.size()))
			{
				source_indices[source_count++] = enemy_id;
				total_hit_point += slot.Entity.GetHitPoint();
			}
		}
		if (source_count <= 0)
		{
			return;
		}

		const float split_threshold = g_BossSplitStage == 0 ?
			boss_data.MaxHitPoint * 0.5f : boss_data.MaxHitPoint * 0.25f;
		if (total_hit_point > split_threshold)
		{
			return;
		}

		int free_count = 0;
		for (int enemy_id = 0; enemy_id < ENEMY_MAX && free_count < source_count; ++enemy_id)
		{
			if (!g_EnemySlots[enemy_id].Entity.IsActive())
			{
				free_indices[free_count++] = enemy_id;
			}
		}
		if (free_count < source_count)
		{
			return;
		}

		const float scale_multiplier = g_BossSplitStage == 0 ?
			BOSS_BODY_SPLIT_SCALE : BOSS_BODY_SECOND_SPLIT_SCALE;
		const float split_offset = g_BossSplitStage == 0 ?
			BOSS_BODY_SPLIT_OFFSET : BOSS_BODY_SECOND_SPLIT_OFFSET;
		const float speed_scale = g_BossSplitStage == 0 ?
			BOSS_BODY_SPLIT_SPEED_SCALE : BOSS_BODY_SECOND_SPLIT_SPEED_SCALE;
		const float burst_intensity = g_BossSplitStage == 0 ? 3.0f : 2.2f;

		for (int source_number = 0; source_number < source_count; ++source_number)
		{
			EnemySlot& source_slot = g_EnemySlots[source_indices[source_number]];
			const DirectX::XMFLOAT2 source_position = source_slot.Entity.GetPosition();
			const float split_max_hit_point =
				source_slot.Entity.GetMaxHitPoint() * 0.5f;
			const float split_hit_point = source_slot.Entity.GetHitPoint() * 0.5f;
			const float split_radius = source_slot.Entity.GetCollisionRadius() *
				scale_multiplier;
			const int room_index = source_slot.RoomIndex;
			const float draw_scale = source_slot.DrawScale * scale_multiplier;
			const float experience_scale = source_slot.ExperienceScale * 0.5f;
			const MonsterMapCollisionShape map_collision =
				GetMonsterMapCollisionShape(MonsterType::BossSlime, draw_scale);

			DirectX::XMFLOAT2 tangent{ 1.0f, 0.0f };
			const float to_player_x = player_position.x - source_position.x;
			const float to_player_y = player_position.y - source_position.y;
			const float to_player_length_squared =
				to_player_x * to_player_x + to_player_y * to_player_y;
			if (to_player_length_squared > 0.0001f)
			{
				const float inverse_length = 1.0f / std::sqrt(to_player_length_squared);
				tangent = {
					-to_player_y * inverse_length,
					to_player_x * inverse_length,
				};
			}
			if ((source_number & 1) != 0)
			{
				tangent.x = -tangent.x;
				tangent.y = -tangent.y;
			}

			const DirectX::XMFLOAT2 split_movement{
				tangent.x * split_offset,
				tangent.y * split_offset,
			};
			const DirectX::XMFLOAT2 first_position = ProceduralMap_MoveActorCircle(
				source_position,
				split_movement,
				split_radius + ENEMY_WALL_PADDING);
			const DirectX::XMFLOAT2 second_position = ProceduralMap_MoveActorCircle(
				source_position,
				{ -split_movement.x, -split_movement.y },
				split_radius + ENEMY_WALL_PADDING);

			source_slot.Entity.Deactivate();
			auto spawn_half = [&](EnemySlot& slot, const DirectX::XMFLOAT2& position,
				const DirectX::XMFLOAT2& outward_direction)
			{
				slot.Entity.Spawn(
					position,
					boss_data.MoveSpeed * speed_scale,
					split_max_hit_point,
					split_radius,
					map_collision.Offset,
					map_collision.Radius);
				slot.Entity.SetHitPoint(split_hit_point);
				slot.Entity.ApplyKnockback(outward_direction, 180.0f);
				slot.Type = MonsterType::BossSlime;
				slot.RoomIndex = room_index;
				slot.DrawScale = draw_scale;
				slot.ExperienceScale = experience_scale;
				slot.BossFireScaleElapsed = BOSS_JELLY_FIRE_SCALE_DURATION;
				slot.BossSplitScaleElapsed = 0.0f;
				EnemyAttackPattern::OnSpawn(
					GetEnemySlotID(&slot), MonsterType::BossSlime);
			};
			spawn_half(source_slot, first_position, tangent);
			spawn_half(
				g_EnemySlots[free_indices[source_number]],
				second_position,
				{ -tangent.x, -tangent.y });

			cSlimeGoo::GetInstance().Spawn(
				source_position, tangent, burst_intensity);
			cSlimeGoo::GetInstance().Spawn(
				source_position,
				{ -tangent.x, -tangent.y },
				burst_intensity);
		}

		++g_BossSplitStage;
		g_BossVolleySequence = 0;
		g_BossJellyFireCooldown = 0.75f;
		g_BossJellyTelegraphEnemyID = -1;
	}

	void UpdateBossJellyPattern(
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		EnemySlot* boss_slot = FindAliveBossSlot();
		if (!boss_slot)
		{
			g_BossJellyFireCooldown = BOSS_JELLY_FIRE_INTERVAL;
			g_BossJellyTelegraphEnemyID = -1;
		}
		else
		{
			const bool phase_two = g_BossSplitStage > 0 ||
				boss_slot->Entity.GetHitPointRatio() <= 0.5f;
			g_BossJellyFireCooldown -= delta_time;
			if (g_BossJellyTelegraphEnemyID < 0 &&
				g_BossJellyFireCooldown <= BOSS_JELLY_TELEGRAPH_DURATION)
			{
				BeginBossJellyTelegraph(player_position);
			}
			if (g_BossJellyFireCooldown <= 0.0f)
			{
				if (IsValidEnemyID(g_BossJellyTelegraphEnemyID))
				{
					EnemySlot& firing_boss =
						g_EnemySlots[g_BossJellyTelegraphEnemyID];
					if (firing_boss.Type == MonsterType::BossSlime &&
						firing_boss.Entity.IsAlive())
					{
						FireBossJellyBullet(
							firing_boss, g_BossJellyTelegraphDirection, phase_two);
					}
				}
				g_BossJellyTelegraphEnemyID = -1;
				g_BossJellyFireCooldown += phase_two ?
					BOSS_JELLY_PHASE_TWO_FIRE_INTERVAL : BOSS_JELLY_FIRE_INTERVAL;
			}
		}

		for (BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (!bullet.IsActive)
			{
				continue;
			}

			if (std::abs(bullet.TurnRate) > 0.0001f)
			{
				bullet.Velocity = RotateDirection(
					bullet.Velocity, bullet.TurnRate * delta_time);
			}
			if (std::abs(bullet.Acceleration) > 0.0001f)
			{
				const float speed = std::sqrt(
					bullet.Velocity.x * bullet.Velocity.x +
					bullet.Velocity.y * bullet.Velocity.y);
				if (speed > 0.0001f)
				{
					const float next_speed = std::max(
						40.0f, speed + bullet.Acceleration * delta_time);
					const float speed_scale = next_speed / speed;
					bullet.Velocity.x *= speed_scale;
					bullet.Velocity.y *= speed_scale;
				}
			}
			bullet.Rotation = std::atan2(
				bullet.Velocity.y, bullet.Velocity.x);

			const DirectX::XMFLOAT2 previous_position = bullet.Position;
			const DirectX::XMFLOAT2 next_position{
				previous_position.x + bullet.Velocity.x * delta_time,
				previous_position.y + bullet.Velocity.y * delta_time,
			};
			const float step_x = next_position.x - previous_position.x;
			const float step_y = next_position.y - previous_position.y;
			const float step_distance = std::sqrt(step_x * step_x + step_y * step_y);
			const bool hit_wall = !ProceduralMap_IsSegmentWalkable(
				previous_position, next_position, bullet.Radius);
			const bool reached_range =
				bullet.Travelled + step_distance >= bullet.MaxDistance;
			if (hit_wall || reached_range)
			{
				const bool should_split = bullet.IsPrimary;
				const DirectX::XMFLOAT2 split_position = previous_position;
				bullet.IsActive = false;
				if (should_split)
				{
					SplitBossJellyBullet(split_position);
				}
				continue;
			}

			bullet.Position = next_position;
			bullet.Travelled += step_distance;
		}
	}

	void GetMonsterFrameRegion(
		MonsterType type,
		float animation_elapsed,
		int& frame_x,
		int& frame_y)
	{
		const MonsterData& data = GetMonsterData(type);
		const int animation_frame = static_cast<int>(
			animation_elapsed / data.AnimationSpeed) % data.FrameCount;
		frame_x = animation_frame * data.FrameWidth;
		frame_y = 0;
	}

	void SpawnBoneDrop(const DirectX::XMFLOAT2& position)
	{
		auto next_random = []()
		{
			g_BoneRandomState ^= g_BoneRandomState << 13;
			g_BoneRandomState ^= g_BoneRandomState >> 17;
			g_BoneRandomState ^= g_BoneRandomState << 5;
			return static_cast<float>(g_BoneRandomState & 0x00ffffffu) /
				static_cast<float>(0x00ffffffu);
		};

		for (int i = 0; i < BONE_BURST_PARTICLE_COUNT; ++i)
		{
			const float angle = next_random() * DirectX::XM_2PI;
			const float speed = 130.0f + next_random() * 150.0f;
			BoneDropEffect bone{};
			bone.Position = {
				position.x + (next_random() * 2.0f - 1.0f) * 8.0f,
				position.y + (next_random() * 2.0f - 1.0f) * 10.0f,
			};
			bone.Velocity = {
				std::cos(angle) * speed,
				std::sin(angle) * speed - 55.0f,
			};
			bone.Size = 24.0f + next_random() * 10.0f;
			bone.Rotation = next_random() * DirectX::XM_2PI;
			bone.AngularVelocity = (next_random() * 2.0f - 1.0f) * 12.0f;
			bone.Lifetime = 0.72f + next_random() * 0.38f;
			bone.Frame = i % BONE_DROP_FRAME_COUNT;
			g_BoneDropEffects.push_back(bone);
		}
	}

	void PlayDamageReaction(
		MonsterType type,
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& impact_direction,
		bool was_killed)
	{
		if (type == MonsterType::Bat)
		{
			const int bat_audio_id = was_killed ?
				g_BatDeathAudioID : g_BatDamageAudioID;
			if (bat_audio_id >= 0)
			{
				PlayAudio(bat_audio_id);
			}
		}
		else if (IsOrcType(type))
		{
			if (was_killed)
			{
				if (g_OrcDeathAudioID >= 0)
				{
					PlayAudio(g_OrcDeathAudioID);
				}
			}
			else
			{
				g_OrcDamageSoundRandomState = Hash32(
					g_OrcDamageSoundRandomState + 0x9e3779b9u);
				const int sound_index = static_cast<int>(
					g_OrcDamageSoundRandomState % g_OrcDamageAudioIDs.size());
				if (g_OrcDamageAudioIDs[sound_index] >= 0)
				{
					PlayAudio(g_OrcDamageAudioIDs[sound_index]);
				}
			}
		}

		const MonsterDeathEffect death_effect =
			GetMonsterData(type).DeathEffect;
		if (death_effect == MonsterDeathEffect::Slime)
		{
			const int slime_audio_id = was_killed ?
				g_SlimeDeathAudioID : g_SlimeDamageAudioID;
			if (slime_audio_id >= 0)
			{
				PlayAudio(slime_audio_id);
			}
			cSlimeGoo::GetInstance().Spawn(
				position,
				impact_direction,
				was_killed ? 1.65f : 1.0f);
			return;
		}

		if (death_effect == MonsterDeathEffect::Bones)
		{
			const int bones_audio_id = was_killed ?
				g_BonesDeathAudioID : g_BonesDamageAudioID;
			if (bones_audio_id >= 0)
			{
				PlayAudio(bones_audio_id);
			}
			if (was_killed)
			{
				SpawnBoneDrop(position);
			}
			return;
		}

		Blood::Spawn(position);
		if (was_killed)
		{
			// Death gets a denser burst without complicating the blood system API.
			Blood::Spawn(position);
		}
	}

	void PlayDashSlashImpact(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& direction,
		int hit_index)
	{
		constexpr std::array<float, DASH_SLASH_HIT_COUNT> CROSS_ANGLES{
			0.58f, 0.72f, 0.88f, 0.68f, DirectX::XM_PIDIV4,
		};
		constexpr std::array<float, DASH_SLASH_HIT_COUNT> NORMAL_OFFSETS{
			-8.0f, 8.0f, -5.0f, 5.0f, 0.0f,
		};
		constexpr std::array<float, DASH_SLASH_HIT_COUNT> BURST_SCALES{
			0.72f, 0.8f, 0.88f, 0.98f, 1.18f,
		};
		constexpr std::array<float, DASH_SLASH_HIT_COUNT> CUT_SCALES{
			0.62f, 0.68f, 0.74f, 0.82f, 0.94f,
		};

		const int index = std::clamp(hit_index, 0, DASH_SLASH_HIT_COUNT - 1);
		const float base_angle = std::atan2(direction.y, direction.x);
		const DirectX::XMFLOAT2 normal{ -direction.y, direction.x };
		const DirectX::XMFLOAT2 impact_center{
			position.x + normal.x * NORMAL_OFFSETS[index],
			position.y + normal.y * NORMAL_OFFSETS[index],
		};

		cGameEffectManager& effects = cGameEffectManager::GetInstance();
		const bool is_finisher = index == DASH_SLASH_HIT_COUNT - 1;
		const DirectX::XMFLOAT4 burst_color = is_finisher ?
			DirectX::XMFLOAT4{ 0.94f, 0.99f, 1.0f, 1.0f } :
			(index % 2 == 0 ?
				DirectX::XMFLOAT4{ 0.34f, 0.92f, 1.0f, 0.96f } :
				DirectX::XMFLOAT4{ 0.68f, 0.32f, 1.0f, 0.96f });
		effects.Play(
			GameEffectType::DashSlashHitBurst,
			impact_center,
			BURST_SCALES[index],
			burst_color,
			base_angle + static_cast<float>(index) * 0.31f);

		// Two compact arcs cross directly over the enemy. Their opening angle
		// changes on every pulse, so accumulated hits bloom into a jagged star.
		for (int side = -1; side <= 1; side += 2)
		{
			const DirectX::XMFLOAT2 cut_position{
				impact_center.x + normal.x * static_cast<float>(side) * 4.0f,
				impact_center.y + normal.y * static_cast<float>(side) * 4.0f,
			};
			effects.Play(
				GameEffectType::DashSlashHitCut,
				cut_position,
				CUT_SCALES[index],
				{ 1.0f, 1.0f, 1.0f, side < 0 ? 0.82f : 1.0f },
				base_angle + CROSS_ANGLES[index] * static_cast<float>(side));
		}

		if (is_finisher)
		{
			for (int quarter_turn = 0; quarter_turn < 2; ++quarter_turn)
			{
				effects.Play(
					GameEffectType::DashSlashHitCut,
					position,
					1.08f,
					{ 0.86f, 0.96f, 1.0f, 0.94f },
					base_angle + DirectX::XM_PIDIV2 *
						static_cast<float>(quarter_turn));
			}
		}
	}

	void ApplyDashSlashPulse(PendingDashSlashAttack& attack)
	{
		const int hit_index = std::clamp(
			attack.NextHitIndex, 0, DASH_SLASH_HIT_COUNT - 1);
		if (g_DashSlashHitAudioIDs[hit_index] >= 0)
		{
			PlayAudio(g_DashSlashHitAudioIDs[hit_index]);
		}

		for (PendingDashSlashTarget& target : attack.Targets)
		{
			const bool target_alive = IsValidEnemyID(target.EnemyID) &&
				g_EnemySlots[target.EnemyID].Entity.IsAlive();
			if (target_alive)
			{
				target.LastPosition = GetEnemyAimPosition(
					g_EnemySlots[target.EnemyID]);
			}

			GameDamageText::Spawn(attack.Damage, target.LastPosition);
			if (target_alive)
			{
				EnemySlot& slot = g_EnemySlots[target.EnemyID];
				ApplyCombatKnockback(
					slot,
					attack.Direction,
					DASH_SLASH_KNOCKBACK_SPEED);
				slot.Entity.ApplyDamage(attack.Damage);
				const bool was_killed = !slot.Entity.IsAlive();
				PlayDamageReaction(
					slot.Type,
					target.LastPosition,
					attack.Direction,
					was_killed);
				if (was_killed && !target.RewardGranted)
				{
					target.RewardGranted = true;
					cGameEffectManager::GetInstance().PlayEnemyDefeat(
						target.LastPosition);
					GameExperienceGem::Spawn(
						slot.Entity.GetPosition(),
						GetEnemyExperienceDrop(slot),
						slot.RoomIndex);
					GameHealingItem::TrySpawn(
						slot.Entity.GetPosition(),
						slot.RoomIndex);
				}
			}

			PlayDashSlashImpact(
				target.LastPosition,
				attack.Direction,
				attack.NextHitIndex);
		}
	}

	int FindFreeEnemySlot()
	{
		for (int i = 0; i < ENEMY_MAX; ++i)
		{
			if (!g_EnemySlots[i].Entity.IsActive())
			{
				return i;
			}
		}
		return -1;
	}

	int GetFreeEnemySlotCount()
	{
		int count = 0;
		for (const EnemySlot& slot : g_EnemySlots)
		{
			if (!slot.Entity.IsActive())
			{
				++count;
			}
		}
		return count;
	}

	void ClearPendingEnemySpawns(int room_index)
	{
		std::erase_if(g_PendingEnemySpawns, [room_index](const PendingEnemySpawn& spawn)
		{
			return spawn.RoomIndex == room_index;
		});
	}

	int EnemyCollisionWorldToCell(float value)
	{
		return static_cast<int>(std::floor(value / g_EnemyCollisionCellSize));
	}

	int EnemyCollisionHashCell(int cell_x, int cell_y)
	{
		const std::uint32_t hash =
			static_cast<std::uint32_t>(cell_x) * 0x8da6b343u ^
			static_cast<std::uint32_t>(cell_y) * 0xd8163841u;
		return static_cast<int>(hash % ENEMY_COLLISION_GRID_BUCKET_COUNT);
	}

	void BuildEnemyCollisionGrid()
	{
		std::fill_n(
			g_EnemyCollisionGrid.BucketHeads,
			ENEMY_COLLISION_GRID_BUCKET_COUNT,
			ENEMY_COLLISION_GRID_INVALID_INDEX);
		std::fill_n(
			g_EnemyCollisionGrid.NextEnemy,
			ENEMY_MAX,
			ENEMY_COLLISION_GRID_INVALID_INDEX);
		g_EnemyCollisionGrid.HasAliveEnemy = false;

		for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
		{
			const cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
			if (!enemy.IsAlive() || !EnemyAttackPattern::IsTargetable(enemy_id))
			{
				continue;
			}

			const DirectX::XMFLOAT2 position =
				GetEnemyAimPosition(g_EnemySlots[enemy_id]);
			const int cell_x = EnemyCollisionWorldToCell(position.x);
			const int cell_y = EnemyCollisionWorldToCell(position.y);
			if (!g_EnemyCollisionGrid.HasAliveEnemy)
			{
				g_EnemyCollisionGrid.MinCellX = cell_x;
				g_EnemyCollisionGrid.MaxCellX = cell_x;
				g_EnemyCollisionGrid.MinCellY = cell_y;
				g_EnemyCollisionGrid.MaxCellY = cell_y;
				g_EnemyCollisionGrid.HasAliveEnemy = true;
			}
			else
			{
				g_EnemyCollisionGrid.MinCellX =
					std::min(g_EnemyCollisionGrid.MinCellX, cell_x);
				g_EnemyCollisionGrid.MaxCellX =
					std::max(g_EnemyCollisionGrid.MaxCellX, cell_x);
				g_EnemyCollisionGrid.MinCellY =
					std::min(g_EnemyCollisionGrid.MinCellY, cell_y);
				g_EnemyCollisionGrid.MaxCellY =
					std::max(g_EnemyCollisionGrid.MaxCellY, cell_y);
			}
			const int bucket = EnemyCollisionHashCell(cell_x, cell_y);
			g_EnemyCollisionGrid.CellX[enemy_id] = cell_x;
			g_EnemyCollisionGrid.CellY[enemy_id] = cell_y;
			g_EnemyCollisionGrid.NextEnemy[enemy_id] =
				g_EnemyCollisionGrid.BucketHeads[bucket];
			g_EnemyCollisionGrid.BucketHeads[bucket] = enemy_id;
		}
	}

	void CheckNearestEnemyInCell(
		int cell_x,
		int cell_y,
		const DirectX::XMFLOAT2& origin,
		bool& found,
		float& best_distance_sq,
		DirectX::XMFLOAT2& best_position)
	{
		const int bucket = EnemyCollisionHashCell(cell_x, cell_y);
		for (int enemy_id = g_EnemyCollisionGrid.BucketHeads[bucket];
			enemy_id != ENEMY_COLLISION_GRID_INVALID_INDEX;
			enemy_id = g_EnemyCollisionGrid.NextEnemy[enemy_id])
		{
			if (g_EnemyCollisionGrid.CellX[enemy_id] != cell_x ||
				g_EnemyCollisionGrid.CellY[enemy_id] != cell_y ||
				!g_EnemySlots[enemy_id].Entity.IsAlive())
			{
				continue;
			}

			const DirectX::XMFLOAT2 position =
				GetEnemyAimPosition(g_EnemySlots[enemy_id]);
			const float delta_x = position.x - origin.x;
			const float delta_y = position.y - origin.y;
			const float distance_sq = delta_x * delta_x + delta_y * delta_y;
			if (!found || distance_sq < best_distance_sq)
			{
				found = true;
				best_distance_sq = distance_sq;
				best_position = position;
			}
		}
	}

	bool ResolveEnemyPair(int enemy_id_a, int enemy_id_b)
	{
		cEnemy& enemy_a = g_EnemySlots[enemy_id_a].Entity;
		cEnemy& enemy_b = g_EnemySlots[enemy_id_b].Entity;
		const DirectX::XMFLOAT2 position_a =
			GetEnemyAimPosition(g_EnemySlots[enemy_id_a]);
		const DirectX::XMFLOAT2 position_b =
			GetEnemyAimPosition(g_EnemySlots[enemy_id_b]);
		float delta_x = position_b.x - position_a.x;
		float delta_y = position_b.y - position_a.y;
		const float distance_sq = delta_x * delta_x + delta_y * delta_y;
		const float collision_distance =
			enemy_a.GetCollisionRadius() + enemy_b.GetCollisionRadius();
		if (distance_sq >= collision_distance * collision_distance)
		{
			return false;
		}

		float distance = 0.0f;
		if (distance_sq > 0.0001f)
		{
			distance = std::sqrt(distance_sq);
			delta_x /= distance;
			delta_y /= distance;
		}
		else
		{
			// Give perfectly stacked enemies a stable, deterministic direction.
			delta_x = ((enemy_id_a + enemy_id_b) & 1) == 0 ? 1.0f : 0.0f;
			delta_y = delta_x == 0.0f ? 1.0f : 0.0f;
		}

		const float correction = (collision_distance - distance) * 0.5f;
		enemy_a.ApplySeparation({ -delta_x * correction, -delta_y * correction });
		enemy_b.ApplySeparation({ delta_x * correction, delta_y * correction });
		return true;
	}

	void ResolveEnemyOverlaps()
	{
		for (int iteration = 0; iteration < ENEMY_COLLISION_SOLVER_ITERATIONS; ++iteration)
		{
			BuildEnemyCollisionGrid();
			bool found_overlap = false;
			for (int i = 0; i < ENEMY_MAX; ++i)
			{
				if (!g_EnemySlots[i].Entity.IsAlive())
				{
					continue;
				}

				const int center_cell_x = g_EnemyCollisionGrid.CellX[i];
				const int center_cell_y = g_EnemyCollisionGrid.CellY[i];
				for (int offset_y = -1; offset_y <= 1; ++offset_y)
				{
					for (int offset_x = -1; offset_x <= 1; ++offset_x)
					{
						const int cell_x = center_cell_x + offset_x;
						const int cell_y = center_cell_y + offset_y;
						const int bucket = EnemyCollisionHashCell(cell_x, cell_y);
						for (int j = g_EnemyCollisionGrid.BucketHeads[bucket];
							j != ENEMY_COLLISION_GRID_INVALID_INDEX;
							j = g_EnemyCollisionGrid.NextEnemy[j])
						{
							if (j <= i ||
								g_EnemyCollisionGrid.CellX[j] != cell_x ||
								g_EnemyCollisionGrid.CellY[j] != cell_y)
							{
								continue;
							}
							found_overlap = ResolveEnemyPair(i, j) || found_overlap;
						}
					}
				}
			}

			if (!found_overlap)
			{
				break;
			}
		}
	}

	bool IsFarEnoughFromEnemies(
		const DirectX::XMFLOAT2& position,
		float collision_radius,
		const std::vector<SpawnPlacement>& new_positions)
	{
		for (const SpawnPlacement& other : new_positions)
		{
			const float minimum_distance =
				collision_radius + other.CollisionRadius + ENEMY_SEPARATION_PADDING;
			const float dx = position.x - other.Position.x;
			const float dy = position.y - other.Position.y;
			if (dx * dx + dy * dy < minimum_distance * minimum_distance)
			{
				return false;
			}
		}
		for (const EnemySlot& slot : g_EnemySlots)
		{
			if (!slot.Entity.IsActive())
			{
				continue;
			}
			const DirectX::XMFLOAT2 other =
				slot.Entity.GetMapCollisionCenter();
			const float minimum_distance =
				collision_radius + slot.Entity.GetMapCollisionRadius() +
				ENEMY_SEPARATION_PADDING;
			const float dx = position.x - other.x;
			const float dy = position.y - other.y;
			if (dx * dx + dy * dy < minimum_distance * minimum_distance)
			{
				return false;
			}
		}
		return true;
	}

	int GetActiveCountInRoom(int room_index)
	{
		int count = 0;
		for (const EnemySlot& slot : g_EnemySlots)
		{
			if (slot.RoomIndex == room_index && slot.Entity.IsActive())
			{
				++count;
			}
		}
		return count;
	}

	bool IsEncounterInProgress(RoomWaveState state)
	{
		return state == RoomWaveState::Waiting ||
			state == RoomWaveState::Telegraphing ||
			state == RoomWaveState::Active;
	}

	void CompleteRoomEncounter(int room_index)
	{
		if (room_index >= 0 && room_index < static_cast<int>(g_RoomWaves.size()))
		{
			RoomWaveRuntime& wave = g_RoomWaves[room_index];
			wave.State = RoomWaveState::Cleared;
			wave.DelayRemaining = 0.0f;
			wave.SpawnRetryCount = 0;
			if (room_index == ProceduralMap_GetFinalEncounterRoomIndex())
			{
				g_RoundCleared = true;
			}
			GameExperienceGem::AttractAllInRoom(room_index);
		}
		ClearPendingEnemySpawns(room_index);
		ProceduralMap_ClearEncounterLock(room_index);
	}

	void AbortRoomEncounter(int room_index)
	{
		if (room_index >= 0 && room_index < static_cast<int>(g_RoomWaves.size()))
		{
			g_RoomWaves[room_index] = RoomWaveRuntime{};
		}
		ClearPendingEnemySpawns(room_index);
		ProceduralMap_ClearEncounterLock(room_index);
	}

	void AbandonOtherActiveRooms(int entered_room_index)
	{
		for (EnemySlot& slot : g_EnemySlots)
		{
			if (slot.Entity.IsActive() && slot.RoomIndex != entered_room_index)
			{
				slot.Entity.Deactivate();
				slot.RoomIndex = -1;
			}
		}
		for (int room_index = 0; room_index < static_cast<int>(g_RoomWaves.size()); ++room_index)
		{
			RoomWaveRuntime& wave = g_RoomWaves[room_index];
			if (room_index != entered_room_index && IsEncounterInProgress(wave.State))
			{
				ProceduralMap_ClearEncounterLock(room_index);
				wave = RoomWaveRuntime{};
			}
		}
		std::erase_if(g_PendingEnemySpawns, [entered_room_index](const PendingEnemySpawn& spawn)
		{
			return spawn.RoomIndex != entered_room_index;
		});
	}

	int GetWaveTargetCount(const ProceduralMapRoom& room, const RoomWaveRuntime& wave)
	{
		const RoundEncounterData& encounter =
			GetMapData().GetRoundEncounter(ProceduralMap_GetRound());
		const int enemy_count_range =
			encounter.MaxEnemiesPerRoom - encounter.MinEnemiesPerRoom + 1;
		int encounter_total = encounter.MinEnemiesPerRoom + static_cast<int>(Hash32(
			ProceduralMap_GetSeed() ^
			static_cast<std::uint32_t>(room.Index + 1) * 0x165667b1u) %
			static_cast<std::uint32_t>(enemy_count_range));
		if (room.IsLargeRoom)
		{
			encounter_total += encounter.LargeRoomEnemyBonus;
		}
		encounter_total = std::max(encounter_total, wave.WaveCount);

		const int base_count = encounter_total / wave.WaveCount;
		const int remainder = encounter_total % wave.WaveCount;
		const int first_larger_wave = wave.WaveCount - remainder;
		const int wave_target = base_count +
			(wave.WaveIndex >= first_larger_wave ? 1 : 0);
		return wave_target;
	}

	void BeginRoomEncounter(
		int room_index,
		const DirectX::XMFLOAT2& player_position)
	{
		if (room_index < 0 || room_index >= static_cast<int>(g_RoomWaves.size()) ||
			g_RoomWaves[room_index].State != RoomWaveState::Dormant)
		{
			return;
		}

		RoomWaveRuntime& wave = g_RoomWaves[room_index];
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room)
		{
			AbortRoomEncounter(room_index);
			return;
		}
		if (room->IsPortalRoom)
		{
			CompleteRoomEncounter(room_index);
			return;
		}
		if (room_index == ProceduralMap_GetStartRoomIndex() &&
			room_index != ProceduralMap_GetFinalEncounterRoomIndex())
		{
			CompleteRoomEncounter(room_index);
			return;
		}
		if (player_position.x < room->WorldMin.x + ENCOUNTER_ENTRY_INSET ||
			player_position.x > room->WorldMax.x - ENCOUNTER_ENTRY_INSET ||
			player_position.y < room->WorldMin.y + ENCOUNTER_ENTRY_INSET ||
			player_position.y > room->WorldMax.y - ENCOUNTER_ENTRY_INSET)
		{
			return;
		}

		const std::uint32_t encounter_hash = Hash32(
			ProceduralMap_GetSeed() ^
			static_cast<std::uint32_t>(room_index + 1) * 0xa511e9b3u);
		const RoundEncounterData& encounter =
			GetMapData().GetRoundEncounter(ProceduralMap_GetRound());
		const int wave_count_range =
			encounter.MaxWaveCount - encounter.MinWaveCount + 1;
		wave.WaveIndex = 0;
		wave.WaveCount = IsBossEncounterRoom(room_index) ? 1 :
			(room->IsLargeRoom ? encounter.LargeRoomWaveCount :
				encounter.MinWaveCount + static_cast<int>(encounter_hash %
					static_cast<std::uint32_t>(wave_count_range)));
		wave.SpawnRetryCount = 0;
		wave.DelayRemaining = 0.0f;

		if (!ProceduralMap_LockEncounterRoom(room_index))
		{
			AbortRoomEncounter(room_index);
			return;
		}
		wave.State = RoomWaveState::Waiting;
	}

	int PrepareCurrentWave(
		int room_index,
		RoomWaveRuntime& wave,
		const DirectX::XMFLOAT2& player_position)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room || wave.WaveCount <= 0 ||
			wave.WaveIndex < 0 || wave.WaveIndex >= wave.WaveCount)
		{
			return 0;
		}

		ClearPendingEnemySpawns(room_index);
		if (IsBossEncounterRoom(room_index))
		{
			if (GetFreeEnemySlotCount() <= 0)
			{
				return 0;
			}
			const MonsterType boss_type =
				GetBossTypeForRound(ProceduralMap_GetRound());
			const MonsterData& boss_data = GetMonsterData(boss_type);
			const MonsterMapCollisionShape map_collision =
				GetMonsterMapCollisionShape(boss_type);
			const float wall_clearance =
				map_collision.Radius + ENEMY_WALL_PADDING;
			DirectX::XMFLOAT2 spawn_position = room->Center;
			if (!ProceduralMap_IsCircleWalkable(
				spawn_position, wall_clearance))
			{
				if (!ProceduralMap_TryGetRoomSpawnPosition(
					room_index,
					7919,
					player_position,
					ENEMY_PLAYER_SPAWN_CLEARANCE,
					wall_clearance,
					spawn_position))
				{
					return 0;
				}
			}
			g_PendingEnemySpawns.push_back({
				spawn_position,
				boss_data.MoveSpeed,
				0.0f,
				boss_type,
				room_index,
			});
			return 1;
		}
		const int target_count = std::min(GetWaveTargetCount(*room, wave), GetFreeEnemySlotCount());
		int prepared_count = 0;

		std::vector<SpawnPlacement> spawned_positions;
		spawned_positions.reserve(target_count);
		const int maximum_attempts = target_count * 28;
		const int wave_sequence_salt = (wave.WaveIndex + 1) * 4099;
		for (int attempt = 0; attempt < maximum_attempts && prepared_count < target_count; ++attempt)
		{
			const std::uint32_t spawn_hash = Hash32(
				ProceduralMap_GetSeed() ^
				static_cast<std::uint32_t>(room_index + 1) * 0x9e3779b9u ^
				static_cast<std::uint32_t>(wave.WaveIndex + 1) * 0xc2b2ae35u ^
				static_cast<std::uint32_t>(prepared_count + 1) * 0x85ebca6bu ^
				static_cast<std::uint32_t>(attempt + 1) * 0x27d4eb2du);
			const MonsterType monster_type = SelectMonsterType(spawn_hash);
			const MonsterData& monster_data = GetMonsterData(monster_type);
			const MonsterMapCollisionShape map_collision =
				GetMonsterMapCollisionShape(monster_type);
			DirectX::XMFLOAT2 spawn_position{};
			const float distance_requirement = attempt < maximum_attempts * 3 / 4 ?
				ENEMY_PLAYER_SPAWN_CLEARANCE : ENEMY_PLAYER_SPAWN_CLEARANCE * 0.65f;
			if (!ProceduralMap_TryGetRoomSpawnPosition(
				room_index,
				wave_sequence_salt + attempt + prepared_count * 37,
				player_position,
				distance_requirement,
				map_collision.Radius + ENEMY_WALL_PADDING,
				spawn_position) ||
				!IsFarEnoughFromEnemies(
					spawn_position,
					map_collision.Radius,
					spawned_positions))
			{
				continue;
			}

			const float speed_amount =
				static_cast<float>(spawn_hash & 0xffffu) / 65535.0f;
			const float speed_variation =
				1.0f + (speed_amount * 2.0f - 1.0f) * ENEMY_SPEED_VARIATION_RATIO;
			const float speed = monster_data.MoveSpeed * speed_variation +
				(room->IsLargeRoom ? 18.0f : 0.0f);

			g_PendingEnemySpawns.push_back({
				spawn_position,
				speed,
				0.0f,
				monster_type,
				room_index,
			});
			spawned_positions.push_back({
				spawn_position,
				map_collision.Radius,
			});
			++prepared_count;
		}

		return prepared_count;
	}

	bool PrepareFallbackEnemy(
		int room_index,
		const RoomWaveRuntime& wave,
		const DirectX::XMFLOAT2& player_position)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room || GetFreeEnemySlotCount() <= 0)
		{
			return false;
		}

		const MonsterType monster_type = IsBossEncounterRoom(room_index) ?
			GetBossTypeForRound(ProceduralMap_GetRound()) : SelectMonsterType(Hash32(
				ProceduralMap_GetSeed() ^
				static_cast<std::uint32_t>(room_index + 1) * 0x9e3779b9u ^
				static_cast<std::uint32_t>(wave.WaveIndex + 1) * 0x85ebca6bu));
		const MonsterData& monster_data = GetMonsterData(monster_type);
		const MonsterMapCollisionShape map_collision =
			GetMonsterMapCollisionShape(monster_type);
		const float wall_clearance =
			map_collision.Radius + ENEMY_WALL_PADDING;
		DirectX::XMFLOAT2 spawn_position{};
		if (!ProceduralMap_TryGetRoomSpawnPosition(
			room_index,
			(wave.WaveIndex + 1) * 7919 + wave.SpawnRetryCount,
			player_position,
			ENEMY_PLAYER_SPAWN_CLEARANCE * 0.35f,
			wall_clearance,
			spawn_position))
		{
			spawn_position = room->Center;
			if (!ProceduralMap_IsCircleWalkable(spawn_position, wall_clearance))
			{
				return false;
			}
		}

		const float speed = monster_data.MoveSpeed +
			(room->IsLargeRoom ? 18.0f : 0.0f);
		ClearPendingEnemySpawns(room_index);
		g_PendingEnemySpawns.push_back({
			spawn_position,
			speed,
			0.0f,
			monster_type,
			room_index,
		});
		return true;
	}

	int SpawnPreparedEnemies(int room_index)
	{
		int spawned_count = 0;
		for (const PendingEnemySpawn& pending : g_PendingEnemySpawns)
		{
			if (pending.RoomIndex != room_index)
			{
				continue;
			}

			const int slot_index = FindFreeEnemySlot();
			if (slot_index < 0)
			{
				break;
			}

			EnemySlot& slot = g_EnemySlots[slot_index];
			const MonsterData& monster_data = GetMonsterData(pending.Type);
			const MonsterMapCollisionShape map_collision =
				GetMonsterMapCollisionShape(pending.Type);
			const DirectX::XMFLOAT2 entity_position{
				pending.Position.x - map_collision.Offset.x,
				pending.Position.y - map_collision.Offset.y,
			};
			slot.Entity.Spawn(
				entity_position,
				pending.Speed,
				monster_data.MaxHitPoint,
				monster_data.CollisionRadius,
				map_collision.Offset,
				map_collision.Radius);
			slot.Type = pending.Type;
			slot.RoomIndex = pending.RoomIndex;
			slot.DrawScale = 1.0f;
			slot.ExperienceScale = 1.0f;
			slot.BossFireScaleElapsed = BOSS_JELLY_FIRE_SCALE_DURATION;
			slot.BossSplitScaleElapsed = BOSS_BODY_SPLIT_SCALE_DURATION;
			EnemyAttackPattern::OnSpawn(slot_index, pending.Type);
			g_SpawnArrivalEffects.push_back({
				pending.Position,
				0.0f,
				GetSpawnTelegraphSize(pending.Type),
			});
			++spawned_count;
		}
		ClearPendingEnemySpawns(room_index);
		return spawned_count;
	}

	void UpdateRoomEncounter(
		int room_index,
		float delta_time,
		const DirectX::XMFLOAT2& player_position)
	{
		if (room_index < 0 || room_index >= static_cast<int>(g_RoomWaves.size()))
		{
			return;
		}

		RoomWaveRuntime& wave = g_RoomWaves[room_index];
		if (wave.State == RoomWaveState::Telegraphing)
		{
			wave.DelayRemaining -= delta_time;
			if (wave.DelayRemaining > 0.0f)
			{
				return;
			}

			if (SpawnPreparedEnemies(room_index) > 0)
			{
				wave.State = RoomWaveState::Active;
				wave.SpawnRetryCount = 0;
				wave.DelayRemaining = 0.0f;
				return;
			}

			wave.State = RoomWaveState::Waiting;
			wave.DelayRemaining = SPAWN_RETRY_DELAY;
			return;
		}

		if (wave.State == RoomWaveState::Waiting)
		{
			wave.DelayRemaining -= delta_time;
			if (wave.DelayRemaining > 0.0f)
			{
				return;
			}

			if (PrepareCurrentWave(room_index, wave, player_position) > 0)
			{
				wave.State = RoomWaveState::Telegraphing;
				wave.SpawnRetryCount = 0;
				wave.DelayRemaining = SPAWN_TELEGRAPH_DURATION;
				return;
			}

			++wave.SpawnRetryCount;
			if (wave.SpawnRetryCount >= MAX_SPAWN_RETRIES)
			{
				if (PrepareFallbackEnemy(room_index, wave, player_position))
				{
					wave.State = RoomWaveState::Telegraphing;
					wave.SpawnRetryCount = 0;
					wave.DelayRemaining = SPAWN_TELEGRAPH_DURATION;
					return;
				}
				// Keep the door closed and retry after a longer backoff. Treating a
				// transient pool/placement failure as a clear would skip the room.
				wave.SpawnRetryCount = 0;
				wave.DelayRemaining = 1.0f;
				return;
			}
			wave.DelayRemaining = SPAWN_RETRY_DELAY;
			return;
		}

		if (wave.State == RoomWaveState::Active && GetActiveCountInRoom(room_index) == 0)
		{
			if (wave.WaveIndex + 1 >= wave.WaveCount)
			{
				CompleteRoomEncounter(room_index);
				return;
			}

			++wave.WaveIndex;
			wave.State = RoomWaveState::Waiting;
			wave.SpawnRetryCount = 0;
			wave.DelayRemaining = BETWEEN_WAVE_DELAY;
		}
	}

	bool TryGetEnemyAndProjectile(
		const cCollisionHit& hit,
		int& enemy_id,
		int& projectile_id)
	{
		if (hit.BodyA.Layer == CollisionLayer::Enemy &&
			hit.BodyB.Layer == CollisionLayer::PlayerBullet)
		{
			enemy_id = hit.BodyA.OwnerID;
			projectile_id = hit.BodyB.OwnerID;
			return true;
		}
		if (hit.BodyA.Layer == CollisionLayer::PlayerBullet &&
			hit.BodyB.Layer == CollisionLayer::Enemy)
		{
			enemy_id = hit.BodyB.OwnerID;
			projectile_id = hit.BodyA.OwnerID;
			return true;
		}
		return false;
	}

	void ApplyProjectileDamage(
		int enemy_id,
		float damage,
		const DirectX::XMFLOAT2& knockback_direction,
		float knockback_speed)
	{
		if (!IsValidEnemyID(enemy_id) || damage <= 0.0f ||
			!g_EnemySlots[enemy_id].Entity.IsAlive())
		{
			return;
		}

		EnemySlot& slot = g_EnemySlots[enemy_id];
		cEnemy& enemy = slot.Entity;
		const DirectX::XMFLOAT2 hit_position = GetEnemyAimPosition(slot);
		GameDamageText::Spawn(damage, hit_position);
		ApplyCombatKnockback(slot, knockback_direction, knockback_speed);
		enemy.ApplyDamage(damage);
		PlayDamageReaction(
			slot.Type,
			hit_position,
			knockback_direction,
			!enemy.IsAlive());
		if (!enemy.IsAlive())
		{
			cGameEffectManager::GetInstance().PlayEnemyDefeat(hit_position);
			GameExperienceGem::Spawn(
				enemy.GetPosition(),
				GetEnemyExperienceDrop(slot),
				slot.RoomIndex);
			GameHealingItem::TrySpawn(
				enemy.GetPosition(),
				slot.RoomIndex);
		}
	}

	void ApplyAreaProjectileDamage(
		const DirectX::XMFLOAT2& center,
		float radius,
		float damage,
		const DirectX::XMFLOAT2& fallback_knockback_direction)
	{
		if (radius <= 0.0f || damage <= 0.0f)
		{
			return;
		}

		GameBullet::PlayFireballExplosionSound();
		cGameEffectManager::GetInstance().PlayAreaExplosion(center, radius);

		for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
		{
			const cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
			if (!enemy.IsAlive())
			{
				continue;
			}
			const float hit_radius = radius + enemy.GetCollisionRadius();
			const float hit_radius_sq = hit_radius * hit_radius;

			const DirectX::XMFLOAT2 enemy_position =
				GetEnemyAimPosition(g_EnemySlots[enemy_id]);
			const DirectX::XMFLOAT2 from_center = {
				enemy_position.x - center.x,
				enemy_position.y - center.y,
			};
			const float distance_sq =
				from_center.x * from_center.x + from_center.y * from_center.y;
			if (distance_sq > hit_radius_sq)
			{
				continue;
			}

			const DirectX::XMFLOAT2 knockback_direction =
				distance_sq > 0.0001f ? from_center : fallback_knockback_direction;
			ApplyProjectileDamage(
				enemy_id,
				damage,
				knockback_direction,
				PLAYER_BULLET_KNOCKBACK_SPEED * 1.35f);
		}
	}

}

namespace GameEnemy
{
void Initialize()
{
	for (int& audio_id : g_DashSlashHitAudioIDs)
	{
		audio_id = LoadAudio(DASH_SLASH_HIT_SOUND_PATH);
	}
	g_SlimeDamageAudioID = LoadAudio(SLIME_DAMAGE_SOUND_PATH);
	g_SlimeDeathAudioID = LoadAudio(SLIME_DEATH_SOUND_PATH);
	g_BossSlimeProjectileFireAudioID = LoadAudio(
		BOSS_SLIME_PROJECTILE_FIRE_SOUND_PATH);
	g_BatDamageAudioID = LoadAudio(BAT_DAMAGE_SOUND_PATH);
	g_BatDeathAudioID = LoadAudio(BAT_DEATH_SOUND_PATH);
	g_BatDashAudioID = LoadAudio(BAT_DASH_SOUND_PATH);
	g_BonesDamageAudioID = LoadAudio(BONES_DAMAGE_SOUND_PATH);
	g_BonesDeathAudioID = LoadAudio(BONES_DEATH_SOUND_PATH);
	for (int& audio_id : g_SkeletonMageFireAudioIDs)
	{
		audio_id = LoadAudio(SKELETON_MAGE_FIRE_SOUND_PATH);
	}
	g_EnemyWarriorSlashAudioID = LoadAudio(ENEMY_WARRIOR_SLASH_SOUND_PATH);
	for (std::size_t i = 0; i < ORC_DAMAGE_SOUND_PATHS.size(); ++i)
	{
		g_OrcDamageAudioIDs[i] = LoadAudio(ORC_DAMAGE_SOUND_PATHS[i]);
	}
	g_OrcDeathAudioID = LoadAudio(ORC_DEATH_SOUND_PATH);
	g_OrcShamanCastAudioID = LoadAudio(ORC_SHAMAN_CAST_SOUND_PATH);
	for (MonsterType type : MONSTER_TYPES)
	{
		const std::size_t type_index = MonsterTypeToIndex(type);
		g_MonsterTextureIDs[type_index] = Texture_Load(
			GetMonsterData(type).TexturePath.c_str(), false);
		g_MonsterTextureSizes[type_index] =
			Texture_GetSize(g_MonsterTextureIDs[type_index]);
	}
	g_EnemyCollisionCellSize = 1.0f;
	for (MonsterType type : MONSTER_TYPES)
	{
		g_EnemyCollisionCellSize = std::max(
			g_EnemyCollisionCellSize,
			GetMonsterData(type).CollisionRadius * 2.0f);
	}
	g_BoneDropTextureID = Texture_Load(
		L"asset/monster/04_skeleton/04_skeleton_white_bones.png", false);
	g_DaggerTextureID = Texture_Load(
		L"asset/dark_rpg_gui/dfgui_icon-sword.png", false);
	g_AxeTextureID = Texture_Load(
		L"asset/dark_rpg_gui/dfgui_icon-axe.png", false);
	g_MageProjectileTextureID = Texture_Load(
		L"asset/texture/vfx/umplix/fireball/fireball.png", false);
	g_SpawnTelegraphTextureID = Texture_Load(
		L"asset/texture/vfx/spawn_magic_circle/magic_circle.png", false);
	g_BossJellyTextureID = Texture_Load(
		L"asset/texture/projectile/venom_comet.png", false);
	g_BossJellyTelegraphTextureID = Texture_Load(
		L"asset/texture/white_square.png", false);
	g_CorruptionProjectileTextureID = Texture_Load(
		L"asset/texture/projectile/void_orb.png", false);
	g_CorruptionAuraTextureID = Texture_Load(
		L"asset/texture/vfx/corruption_flame/corruption_flame_4x2.png", false);
	EnemyAttackPattern::Initialize(
		g_BoneDropTextureID,
		g_DaggerTextureID,
		g_AxeTextureID,
		g_MageProjectileTextureID,
		g_BossJellyTelegraphTextureID,
		g_SpawnTelegraphTextureID,
		g_BonesDamageAudioID,
		g_SkeletonMageFireAudioIDs,
		g_EnemyWarriorSlashAudioID,
		g_BatDashAudioID,
		g_OrcShamanCastAudioID);
	ResetDungeon();
}

void Finalize()
{
	ResetDungeon();
	EnemyAttackPattern::Finalize();
	g_RoomWaves.clear();
	for (int& audio_id : g_DashSlashHitAudioIDs)
	{
		if (audio_id >= 0)
		{
			UnloadAudio(audio_id);
			audio_id = -1;
		}
	}
	if (g_SlimeDamageAudioID >= 0)
	{
		UnloadAudio(g_SlimeDamageAudioID);
		g_SlimeDamageAudioID = -1;
	}
	if (g_SlimeDeathAudioID >= 0)
	{
		UnloadAudio(g_SlimeDeathAudioID);
		g_SlimeDeathAudioID = -1;
	}
	if (g_BossSlimeProjectileFireAudioID >= 0)
	{
		UnloadAudio(g_BossSlimeProjectileFireAudioID);
		g_BossSlimeProjectileFireAudioID = -1;
	}
	if (g_BatDamageAudioID >= 0)
	{
		UnloadAudio(g_BatDamageAudioID);
		g_BatDamageAudioID = -1;
	}
	if (g_BatDeathAudioID >= 0)
	{
		UnloadAudio(g_BatDeathAudioID);
		g_BatDeathAudioID = -1;
	}
	if (g_BatDashAudioID >= 0)
	{
		UnloadAudio(g_BatDashAudioID);
		g_BatDashAudioID = -1;
	}
	if (g_BonesDamageAudioID >= 0)
	{
		UnloadAudio(g_BonesDamageAudioID);
		g_BonesDamageAudioID = -1;
	}
	if (g_BonesDeathAudioID >= 0)
	{
		UnloadAudio(g_BonesDeathAudioID);
		g_BonesDeathAudioID = -1;
	}
	for (int& audio_id : g_SkeletonMageFireAudioIDs)
	{
		if (audio_id >= 0)
		{
			UnloadAudio(audio_id);
			audio_id = -1;
		}
	}
	if (g_EnemyWarriorSlashAudioID >= 0)
	{
		UnloadAudio(g_EnemyWarriorSlashAudioID);
		g_EnemyWarriorSlashAudioID = -1;
	}
	for (int& audio_id : g_OrcDamageAudioIDs)
	{
		if (audio_id >= 0)
		{
			UnloadAudio(audio_id);
			audio_id = -1;
		}
	}
	if (g_OrcDeathAudioID >= 0)
	{
		UnloadAudio(g_OrcDeathAudioID);
		g_OrcDeathAudioID = -1;
	}
	if (g_OrcShamanCastAudioID >= 0)
	{
		UnloadAudio(g_OrcShamanCastAudioID);
		g_OrcShamanCastAudioID = -1;
	}
	for (int& texture_id : g_MonsterTextureIDs)
	{
		Texture_Release(texture_id);
		texture_id = TEXTURE_INVALID_ID;
	}
	Texture_Release(g_BoneDropTextureID);
	g_BoneDropTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_DaggerTextureID);
	g_DaggerTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_AxeTextureID);
	g_AxeTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_MageProjectileTextureID);
	g_MageProjectileTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_SpawnTelegraphTextureID);
	g_SpawnTelegraphTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_BossJellyTextureID);
	g_BossJellyTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_BossJellyTelegraphTextureID);
	g_BossJellyTelegraphTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_CorruptionProjectileTextureID);
	g_CorruptionProjectileTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_CorruptionAuraTextureID);
	g_CorruptionAuraTextureID = TEXTURE_INVALID_ID;
}

void ResetDungeon()
{
	ProceduralMap_ClearEncounterLock();
	g_RoundCleared = false;
	g_MonsterAnimationElapsed = 0.0f;
	g_PendingEnemySpawns.clear();
	g_SpawnArrivalEffects.clear();
	g_BoneDropEffects.clear();
	g_PendingDashSlashAttacks.clear();
	ClearBossJellyBullets();
	g_BossJellyFireCooldown = BOSS_JELLY_FIRE_INTERVAL;
	g_BossJellyTelegraphEnemyID = -1;
	g_BossJellyTelegraphDirection = { 1.0f, 0.0f };
	g_BossSplitStage = 0;
	g_BossVolleySequence = 0;
	g_CorruptionFireCooldown = 0.8f;
	g_CorruptionVolleySequence = 0;
	g_BoneRandomState = 0xB04E5EEDu ^ ProceduralMap_GetSeed();
	g_OrcDamageSoundRandomState = 0x0ACD4A6Eu ^ ProceduralMap_GetSeed();
	for (EnemySlot& slot : g_EnemySlots)
	{
		slot.Entity = cEnemy{};
		slot.Type = DEFAULT_MONSTER_TYPE;
		slot.RoomIndex = -1;
		slot.DrawScale = 1.0f;
		slot.ExperienceScale = 1.0f;
		slot.BossFireScaleElapsed = BOSS_JELLY_FIRE_SCALE_DURATION;
		slot.BossSplitScaleElapsed = BOSS_BODY_SPLIT_SCALE_DURATION;
	}
	EnemyAttackPattern::Reset(ProceduralMap_GetSeed());

	g_RoomWaves.assign(ProceduralMap_GetRoomCount(), RoomWaveRuntime{});
	g_DiscoveredRooms.assign(ProceduralMap_GetRoomCount(), false);
	const int start_room = ProceduralMap_GetStartRoomIndex();
	if (start_room >= 0 && start_room < static_cast<int>(g_DiscoveredRooms.size()))
	{
		g_DiscoveredRooms[start_room] = true;
	}
	if (start_room >= 0 && start_room < static_cast<int>(g_RoomWaves.size()) &&
		start_room != ProceduralMap_GetFinalEncounterRoomIndex())
	{
		g_RoomWaves[start_room].State = RoomWaveState::Cleared;
	}
	for (int room_index = 0; room_index < static_cast<int>(g_RoomWaves.size()); ++room_index)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (room && room->IsPortalRoom)
		{
			g_RoomWaves[room_index].State = RoomWaveState::Cleared;
		}
	}
	g_CurrentRoomIndex = start_room;
	BuildEnemyCollisionGrid();
}

void Update(float delta_time)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	g_MonsterAnimationElapsed = std::fmod(
		g_MonsterAnimationElapsed + safe_delta_time,
		60.0f);
	for (PendingEnemySpawn& pending : g_PendingEnemySpawns)
	{
		pending.Elapsed += safe_delta_time;
	}
	for (SpawnArrivalEffect& arrival : g_SpawnArrivalEffects)
	{
		arrival.Elapsed += safe_delta_time;
	}
	std::erase_if(g_SpawnArrivalEffects, [](const SpawnArrivalEffect& arrival)
	{
		return arrival.Elapsed >= SPAWN_ARRIVAL_RING_DURATION;
	});
	for (BoneDropEffect& bone_drop : g_BoneDropEffects)
	{
		bone_drop.Elapsed += safe_delta_time;
		bone_drop.Velocity.y += BONE_BURST_GRAVITY * safe_delta_time;
		bone_drop.Position.x += bone_drop.Velocity.x * safe_delta_time;
		bone_drop.Position.y += bone_drop.Velocity.y * safe_delta_time;
		const float drag = std::pow(BONE_BURST_DRAG_PER_SECOND, safe_delta_time);
		bone_drop.Velocity.x *= drag;
		bone_drop.Velocity.y *= drag;
		bone_drop.Rotation += bone_drop.AngularVelocity * safe_delta_time;
	}
	std::erase_if(g_BoneDropEffects, [](const BoneDropEffect& bone_drop)
	{
		return bone_drop.Elapsed >= bone_drop.Lifetime;
	});

	const DirectX::XMFLOAT2 player_position = GamePlayer::GetPosition();
	const int room_index = ProceduralMap_GetRoomIndexAt(player_position);
	if (room_index >= 0 && room_index < static_cast<int>(g_DiscoveredRooms.size()))
	{
		g_DiscoveredRooms[room_index] = true;
	}
	if (room_index >= 0 && room_index != g_CurrentRoomIndex)
	{
		AbandonOtherActiveRooms(room_index);
		g_CurrentRoomIndex = room_index;
	}
	if (room_index >= 0)
	{
		BeginRoomEncounter(room_index, player_position);
	}

	for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
	{
		EnemySlot& slot = g_EnemySlots[enemy_id];
		const MonsterData& monster_data = GetMonsterData(slot.Type);
		const float visual_bottom_offset =
			monster_data.DrawSize.y * slot.DrawScale * 0.5f;
		slot.BossFireScaleElapsed = std::min(
			slot.BossFireScaleElapsed + safe_delta_time,
			BOSS_JELLY_FIRE_SCALE_DURATION);
		slot.BossSplitScaleElapsed = std::min(
			slot.BossSplitScaleElapsed + safe_delta_time,
			BOSS_BODY_SPLIT_SCALE_DURATION);
		EnemyAttackPattern::UpdateEnemy(
			enemy_id,
			slot.Type,
			slot.Entity,
			delta_time,
			player_position,
			visual_bottom_offset,
			g_MonsterAnimationElapsed);
		if (!slot.Entity.IsActive())
		{
			slot.RoomIndex = -1;
		}
	}
	TrySplitBossBody(player_position);
	UpdateBossJellyPattern(safe_delta_time, player_position);
	UpdateCorruptedBossPattern(safe_delta_time, player_position);
	EnemyAttackPattern::UpdateProjectiles(safe_delta_time);
	ResolveEnemyOverlaps();

	UpdateRoomEncounter(g_CurrentRoomIndex, delta_time, player_position);
	// Spawning happens during encounter updates, so rebuild once at the end to
	// make the query grid match the positions exposed to auto aim this frame.
	BuildEnemyCollisionGrid();
}

bool FindNearestAlive(
	const DirectX::XMFLOAT2& origin,
	DirectX::XMFLOAT2& out_position)
{
	if (!g_EnemyCollisionGrid.HasAliveEnemy)
	{
		return false;
	}

	const int origin_cell_x = EnemyCollisionWorldToCell(origin.x);
	const int origin_cell_y = EnemyCollisionWorldToCell(origin.y);
	const int max_ring = std::max({
		std::abs(origin_cell_x - g_EnemyCollisionGrid.MinCellX),
		std::abs(origin_cell_x - g_EnemyCollisionGrid.MaxCellX),
		std::abs(origin_cell_y - g_EnemyCollisionGrid.MinCellY),
		std::abs(origin_cell_y - g_EnemyCollisionGrid.MaxCellY),
	});

	bool found = false;
	float best_distance_sq = 0.0f;
	DirectX::XMFLOAT2 best_position{};
	for (int ring = 0; ring <= max_ring; ++ring)
	{
		if (ring == 0)
		{
			CheckNearestEnemyInCell(
				origin_cell_x, origin_cell_y, origin,
				found, best_distance_sq, best_position);
		}
		else
		{
			const int min_cell_x = origin_cell_x - ring;
			const int max_cell_x = origin_cell_x + ring;
			const int min_cell_y = origin_cell_y - ring;
			const int max_cell_y = origin_cell_y + ring;
			for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
			{
				CheckNearestEnemyInCell(
					cell_x, min_cell_y, origin,
					found, best_distance_sq, best_position);
				CheckNearestEnemyInCell(
					cell_x, max_cell_y, origin,
					found, best_distance_sq, best_position);
			}
			for (int cell_y = min_cell_y + 1; cell_y < max_cell_y; ++cell_y)
			{
				CheckNearestEnemyInCell(
					min_cell_x, cell_y, origin,
					found, best_distance_sq, best_position);
				CheckNearestEnemyInCell(
					max_cell_x, cell_y, origin,
					found, best_distance_sq, best_position);
			}
		}

		if (found)
		{
			const float searched_left =
				static_cast<float>(origin_cell_x - ring) * g_EnemyCollisionCellSize;
			const float searched_right =
				static_cast<float>(origin_cell_x + ring + 1) * g_EnemyCollisionCellSize;
			const float searched_top =
				static_cast<float>(origin_cell_y - ring) * g_EnemyCollisionCellSize;
			const float searched_bottom =
				static_cast<float>(origin_cell_y + ring + 1) * g_EnemyCollisionCellSize;
			const float nearest_unsearched_distance = std::min({
				origin.x - searched_left,
				searched_right - origin.x,
				origin.y - searched_top,
				searched_bottom - origin.y,
			});
			if (best_distance_sq <=
				nearest_unsearched_distance * nearest_unsearched_distance)
			{
				break;
			}
		}
	}

	if (!found)
	{
		return false;
	}
	out_position = best_position;
	return true;
}

bool FindNearestAliveForChain(
	const DirectX::XMFLOAT2& origin,
	const int* excluded_enemy_ids,
	int excluded_count,
	float max_distance,
	int& out_enemy_id,
	DirectX::XMFLOAT2& out_position)
{
	if (max_distance <= 0.0f)
	{
		return false;
	}

	bool found = false;
	float best_distance_sq = max_distance * max_distance;
	for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
	{
		const cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
		if (!enemy.IsAlive() || !EnemyAttackPattern::IsTargetable(enemy_id))
		{
			continue;
		}

		bool excluded = false;
		for (int i = 0; i < excluded_count; ++i)
		{
			if (excluded_enemy_ids && excluded_enemy_ids[i] == enemy_id)
			{
				excluded = true;
				break;
			}
		}
		if (excluded)
		{
			continue;
		}

		const DirectX::XMFLOAT2 position =
			GetEnemyAimPosition(g_EnemySlots[enemy_id]);
		const float dx = position.x - origin.x;
		const float dy = position.y - origin.y;
		const float distance_sq = dx * dx + dy * dy;
		if (distance_sq > best_distance_sq)
		{
			continue;
		}

		found = true;
		best_distance_sq = distance_sq;
		out_enemy_id = enemy_id;
		out_position = position;
	}

	return found;
}

bool ApplyChainLightningDamage(int enemy_id, float damage)
{
	if (!IsValidEnemyID(enemy_id) || damage <= 0.0f ||
		!g_EnemySlots[enemy_id].Entity.IsAlive() ||
		!EnemyAttackPattern::IsTargetable(enemy_id))
	{
		return false;
	}

	EnemySlot& slot = g_EnemySlots[enemy_id];
	cEnemy& enemy = slot.Entity;
	const DirectX::XMFLOAT2 hit_position = GetEnemyAimPosition(slot);
	GameDamageText::Spawn(damage, hit_position);
	enemy.ApplyDamage(damage);
	PlayDamageReaction(
		slot.Type,
		hit_position,
		{ 0.0f, 0.0f },
		!enemy.IsAlive());
	if (!enemy.IsAlive())
	{
		cGameEffectManager::GetInstance().PlayEnemyDefeat(hit_position);
		GameExperienceGem::Spawn(
			enemy.GetPosition(),
			GetEnemyExperienceDrop(slot),
			slot.RoomIndex);
		GameHealingItem::TrySpawn(
			enemy.GetPosition(),
			slot.RoomIndex);
	}
	return true;
}

int ApplyDashSlashDamage(
	const DirectX::XMFLOAT2& start,
	const DirectX::XMFLOAT2& end,
	float half_width,
	float damage)
{
	if (half_width <= 0.0f || damage <= 0.0f)
	{
		return 0;
	}

	const float segment_x = end.x - start.x;
	const float segment_y = end.y - start.y;
	const float segment_length_sq =
		segment_x * segment_x + segment_y * segment_y;
	if (segment_length_sq <= 1.0f)
	{
		return 0;
	}

	const float segment_length = std::sqrt(segment_length_sq);
	const DirectX::XMFLOAT2 slash_direction = {
		segment_x / segment_length,
		segment_y / segment_length,
	};
	PendingDashSlashAttack pending_attack{};
	pending_attack.Direction = slash_direction;
	pending_attack.Damage = damage;
	for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
	{
		const cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
		if (!enemy.IsAlive() || !EnemyAttackPattern::IsTargetable(enemy_id))
		{
			continue;
		}

		const DirectX::XMFLOAT2 enemy_position =
			GetEnemyAimPosition(g_EnemySlots[enemy_id]);
		const float relative_x = enemy_position.x - start.x;
		const float relative_y = enemy_position.y - start.y;
		const float amount = std::clamp(
			(relative_x * segment_x + relative_y * segment_y) /
				segment_length_sq,
			0.0f, 1.0f);
		const float nearest_x = start.x + segment_x * amount;
		const float nearest_y = start.y + segment_y * amount;
		const float distance_x = enemy_position.x - nearest_x;
		const float distance_y = enemy_position.y - nearest_y;
		const float hit_radius = half_width + enemy.GetCollisionRadius();
		if (distance_x * distance_x + distance_y * distance_y >
			hit_radius * hit_radius)
		{
			continue;
		}

		pending_attack.Targets.push_back({ enemy_id, enemy_position, false });
	}

	const int target_count = static_cast<int>(pending_attack.Targets.size());
	if (target_count > 0)
	{
		g_PendingDashSlashAttacks.push_back(std::move(pending_attack));
	}
	return target_count;
}

void UpdateDashSlashAttacks(float delta_time)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	for (PendingDashSlashAttack& attack : g_PendingDashSlashAttacks)
	{
		attack.Elapsed += safe_delta_time;
		while (attack.NextHitIndex < DASH_SLASH_HIT_COUNT &&
			attack.Elapsed >= attack.NextHitIndex * DASH_SLASH_HIT_INTERVAL)
		{
			ApplyDashSlashPulse(attack);
			++attack.NextHitIndex;
		}
	}
	std::erase_if(
		g_PendingDashSlashAttacks,
		[](const PendingDashSlashAttack& attack)
		{
			return attack.NextHitIndex >= DASH_SLASH_HIT_COUNT;
		});
}

bool TryGetAlivePosition(
	int enemy_id,
	DirectX::XMFLOAT2& out_position)
{
	if (!IsValidEnemyID(enemy_id) ||
		!g_EnemySlots[enemy_id].Entity.IsAlive() ||
		!EnemyAttackPattern::IsTargetable(enemy_id))
	{
		return false;
	}

	out_position = GetEnemyAimPosition(g_EnemySlots[enemy_id]);
	return true;
}

bool IsRoundCleared()
{
	return g_RoundCleared;
}

bool HasPendingBossSpawn()
{
	return std::any_of(
		g_PendingEnemySpawns.begin(),
		g_PendingEnemySpawns.end(),
		[](const PendingEnemySpawn& pending)
		{
			return pending.Type == MonsterType::BossSlime;
		});
}

bool TryGetBossHealth(float& out_hit_point, float& out_max_hit_point)
{
	out_hit_point = 0.0f;
	out_max_hit_point = 0.0f;
	bool found = false;
	for (const EnemySlot& slot : g_EnemySlots)
	{
		if (!IsBossType(slot.Type) || !slot.Entity.IsActive())
		{
			continue;
		}
		if (!found)
		{
			out_max_hit_point = GetMonsterData(slot.Type).MaxHitPoint;
		}
		out_hit_point += slot.Entity.GetHitPoint();
		found = true;
	}
	return found;
}

const char* GetBossDisplayName()
{
	switch (GetBossTypeForRound(ProceduralMap_GetRound()))
	{
	case MonsterType::BossSlime: return "GIANT SLIME";
	case MonsterType::BossCorruptedKnight: return "CORRUPTED KNIGHT";
	case MonsterType::BossCorruptedMage: return "CORRUPTED MAGE";
	case MonsterType::BossCthulhu: return "ABYSS CTHULHU";
	default: return "CORRUPTED BOSS";
	}
}

bool IsRoomDiscovered(int room_index)
{
	return room_index >= 0 &&
		room_index < static_cast<int>(g_DiscoveredRooms.size()) &&
		g_DiscoveredRooms[room_index];
}

bool IsRoomCleared(int room_index)
{
	return room_index >= 0 &&
		room_index < static_cast<int>(g_RoomWaves.size()) &&
		g_RoomWaves[room_index].State == RoomWaveState::Cleared;
}

void AppendBossTelegraphLine(
	std::vector<SpriteInstance>& instances,
	const DirectX::XMFLOAT2& start,
	const DirectX::XMFLOAT2& end,
	float width,
	const DirectX::XMFLOAT4& color)
{
	const float dx = end.x - start.x;
	const float dy = end.y - start.y;
	const float length_squared = dx * dx + dy * dy;
	if (length_squared <= 0.0001f)
	{
		return;
	}
	const float length = std::sqrt(length_squared);
	instances.push_back({
		{ (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f },
		{ length, width },
		std::atan2(dy, dx),
		color,
		{ 0.0f, 0.0f },
		{ 1.0f, 1.0f },
		1.0f,
	});
}

void DrawBossJellyTelegraph()
{
	if (g_BossJellyTelegraphTextureID == TEXTURE_INVALID_ID ||
		!IsValidEnemyID(g_BossJellyTelegraphEnemyID))
	{
		return;
	}

	const EnemySlot& firing_boss = g_EnemySlots[g_BossJellyTelegraphEnemyID];
	if (firing_boss.Type != MonsterType::BossSlime ||
		!firing_boss.Entity.IsAlive())
	{
		return;
	}

	const float remaining = std::clamp(
		g_BossJellyFireCooldown, 0.0f, BOSS_JELLY_TELEGRAPH_DURATION);
	const float progress = 1.0f - remaining / BOSS_JELLY_TELEGRAPH_DURATION;
	const float pulse = 0.68f + 0.32f * std::abs(
		std::sin(progress * DirectX::XM_PI * 5.0f));
	static std::vector<SpriteInstance> warning_instances;
	warning_instances.clear();
	if (warning_instances.capacity() < 2)
	{
		warning_instances.reserve(2);
	}

	const bool phase_two = g_BossSplitStage > 0 ||
		firing_boss.Entity.GetHitPointRatio() <= 0.5f;
	const int shot_count = phase_two ? 2 : 1;
	for (int shot_index = 0; shot_index < shot_count; ++shot_index)
	{
		const DirectX::XMFLOAT2 direction = GetBossJellyPrimaryDirection(
			g_BossJellyTelegraphDirection, phase_two, shot_index);
		const DirectX::XMFLOAT2 muzzle_position =
			GetBossJellyMuzzlePosition(firing_boss, direction);
		const DirectX::XMFLOAT2 telegraph_end_position = TraceBossJellyPath(
			muzzle_position,
			direction,
			BOSS_JELLY_PRIMARY_DISTANCE,
			30.0f);
		AppendBossTelegraphLine(
			warning_instances,
			muzzle_position,
			telegraph_end_position,
			60.0f,
			{ 1.0f, 0.0f, 0.02f, (0.18f + progress * 0.12f) * pulse });
	}

	SpriteInstanced_DrawUnlit(
		g_BossJellyTelegraphTextureID,
		warning_instances.data(),
		static_cast<int>(warning_instances.size()));
}

void DrawSpawnTelegraphs()
{
	DrawBossJellyTelegraph();
	EnemyAttackPattern::DrawTelegraphs();
	if (g_SpawnTelegraphTextureID == TEXTURE_INVALID_ID ||
		(g_PendingEnemySpawns.empty() && g_SpawnArrivalEffects.empty()))
	{
		return;
	}

	static std::vector<SpriteInstance> telegraph_instances;
	if (telegraph_instances.capacity() < ENEMY_MAX)
	{
		telegraph_instances.reserve(ENEMY_MAX);
	}
	telegraph_instances.clear();
	for (const PendingEnemySpawn& pending : g_PendingEnemySpawns)
	{
		const float progress = std::clamp(
			pending.Elapsed / SPAWN_TELEGRAPH_DURATION, 0.0f, 1.0f);
		const float completion_flash = std::clamp(
			(progress - 0.82f) / 0.18f, 0.0f, 1.0f);
		const float size = GetSpawnTelegraphSize(pending.Type) *
			(0.96f + completion_flash * 0.04f);
		telegraph_instances.push_back({
			pending.Position,
			{ size, size },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 0.78f + completion_flash * 0.22f },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			2.0f + progress,
		});
	}
	for (const SpawnArrivalEffect& arrival : g_SpawnArrivalEffects)
	{
		const float progress = std::clamp(
			arrival.Elapsed / SPAWN_ARRIVAL_RING_DURATION, 0.0f, 1.0f);
		const float smooth_progress = progress * progress * (3.0f - 2.0f * progress);
		const float alpha = (1.0f - progress) * (1.0f - progress) * 0.9f;
		const float size = arrival.BaseSize +
			SPAWN_ARRIVAL_RING_EXPANSION * smooth_progress;
		telegraph_instances.push_back({
			arrival.Position,
			{ size, size },
			0.0f,
			{ 0.72f, 0.92f, 1.0f, alpha },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
		});
	}

	SpriteInstanced_DrawAdditiveUnlit(
		g_SpawnTelegraphTextureID,
		telegraph_instances.data(),
		static_cast<int>(telegraph_instances.size()));
}

void Draw()
{
	if (g_BoneDropTextureID != TEXTURE_INVALID_ID && !g_BoneDropEffects.empty())
	{
		static std::vector<SpriteInstance> bone_instances;
		if (bone_instances.capacity() < ENEMY_MAX)
		{
			bone_instances.reserve(ENEMY_MAX);
		}
		bone_instances.clear();
		for (const BoneDropEffect& bone_drop : g_BoneDropEffects)
		{
			const float progress = bone_drop.Lifetime > 0.0f ?
				std::clamp(bone_drop.Elapsed / bone_drop.Lifetime, 0.0f, 1.0f) : 1.0f;
			const float alpha = progress < 0.55f ? 1.0f :
				(1.0f - progress) / 0.45f;
			const float size = bone_drop.Size * (1.0f - progress * 0.12f);
			bone_instances.push_back({
				bone_drop.Position,
				{ size, size },
				bone_drop.Rotation,
				{ 1.0f, 1.0f, 1.0f, alpha },
				{
					static_cast<float>(bone_drop.Frame * BONE_DROP_FRAME_WIDTH) /
						BONE_DROP_TEXTURE_WIDTH,
					0.0f,
				},
				{
					static_cast<float>(BONE_DROP_FRAME_WIDTH) / BONE_DROP_TEXTURE_WIDTH,
					static_cast<float>(BONE_DROP_FRAME_HEIGHT) / BONE_DROP_TEXTURE_HEIGHT,
				},
			});
		}
		SpriteInstanced_DrawUnlit(
			g_BoneDropTextureID,
			bone_instances.data(),
			static_cast<int>(bone_instances.size()));
	}

	static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_instances;
	static std::array<std::vector<SpriteInstance>,
		MONSTER_TYPE_COUNT> monster_afterimage_instances;
	static std::vector<SpriteInstance> boss_split_glow_instances;
	static std::vector<SpriteInstance> corruption_aura_instances;
	static std::vector<SpriteInstance> corruption_aura_glow_instances;
	boss_split_glow_instances.clear();
	corruption_aura_instances.clear();
	corruption_aura_glow_instances.clear();
	if (boss_split_glow_instances.capacity() < 4)
	{
		boss_split_glow_instances.reserve(4);
	}
	for (std::vector<SpriteInstance>& instances : monster_instances)
	{
		if (instances.capacity() < ENEMY_MAX)
		{
			instances.reserve(ENEMY_MAX);
		}
		instances.clear();
	}
	for (std::vector<SpriteInstance>& instances : monster_afterimage_instances)
	{
		if (instances.capacity() < ENEMY_MAX)
		{
			instances.reserve(ENEMY_MAX);
		}
		instances.clear();
	}

	for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
	{
		const EnemySlot& slot = g_EnemySlots[enemy_id];
		int frame_x = 0;
		int frame_y = 0;
		const float animation_elapsed = EnemyAttackPattern::GetAnimationElapsed(
			enemy_id, g_MonsterAnimationElapsed);
		GetMonsterFrameRegion(
			slot.Type, animation_elapsed, frame_x, frame_y);
		EnemyAttackPattern::GetAnimationFrameRegion(
			enemy_id, frame_x, frame_y);
		const int texture_id = GetMonsterTextureID(slot.Type);
		if (slot.Entity.IsAlive())
		{
			std::vector<SpriteInstance>& instances =
				monster_instances[MonsterTypeToIndex(slot.Type)];
			const MonsterData& data = GetMonsterData(slot.Type);
			const DirectX::XMUINT2 texture_size =
				g_MonsterTextureSizes[MonsterTypeToIndex(slot.Type)];
			const bool flip_horizontal =
				slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
			const DirectX::XMFLOAT2 fire_visual_scale =
				GetBossFireVisualScale(slot);
			const DirectX::XMFLOAT2 split_visual_scale =
				GetBossSplitVisualScale(slot);
			const float draw_width =
				(flip_horizontal ? -data.DrawSize.x : data.DrawSize.x) *
				slot.DrawScale * fire_visual_scale.x * split_visual_scale.x;
			const float draw_height = data.DrawSize.y * slot.DrawScale *
				fire_visual_scale.y * split_visual_scale.y;
			const DirectX::XMFLOAT2 texcoord_offset{
				static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
				static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
			};
			const DirectX::XMFLOAT2 texcoord_scale{
				static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x),
				static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y),
			};
			if (slot.Type == MonsterType::OrcWarrior ||
				slot.Type == MonsterType::OrcRogue)
			{
				EnemyAttackPattern::DashAfterimagePath paths[
					ORC_WARRIOR_DASH_AFTERIMAGE_PATH_CAPACITY]{};
				const int path_count = EnemyAttackPattern::GetDashAfterimagePaths(
					enemy_id,
					paths,
					ORC_WARRIOR_DASH_AFTERIMAGE_PATH_CAPACITY);
				std::vector<SpriteInstance>& afterimages =
					monster_afterimage_instances[MonsterTypeToIndex(slot.Type)];
				for (int path_index = 0; path_index < path_count; ++path_index)
				{
					const EnemyAttackPattern::DashAfterimagePath& path =
						paths[path_index];
					for (int i = 0; i < ORC_WARRIOR_DASH_AFTERIMAGE_COUNT; ++i)
					{
						const float color_amount = static_cast<float>(i) /
							static_cast<float>(ORC_WARRIOR_DASH_AFTERIMAGE_COUNT - 1);
						const float path_amount = static_cast<float>(i) /
							static_cast<float>(ORC_WARRIOR_DASH_AFTERIMAGE_COUNT);
						afterimages.push_back({
							{
								path.Start.x + (path.End.x - path.Start.x) * path_amount,
								path.Start.y + (path.End.y - path.Start.y) * path_amount,
							},
							{ draw_width, draw_height },
							0.0f,
							{
								1.0f, 0.28f + 0.30f * color_amount, 0.12f,
								path.Fade * (0.24f + 0.30f * color_amount),
							},
							texcoord_offset,
							texcoord_scale,
						});
					}
				}
			}
			const float visual_alpha =
				EnemyAttackPattern::GetVisualAlpha(enemy_id);
			instances.push_back({
				slot.Entity.GetPosition(),
				{ draw_width, draw_height },
				0.0f,
				{ 1.0f, 1.0f, 1.0f, visual_alpha },
				texcoord_offset,
				texcoord_scale,
			});

			if (IsCorruptedBossType(slot.Type))
			{
				const int aura_frame = static_cast<int>(
					g_MonsterAnimationElapsed / CORRUPTION_AURA_FRAME_DURATION) %
					CORRUPTION_AURA_FRAME_COUNT;
				const int aura_column = aura_frame % CORRUPTION_AURA_COLUMN_COUNT;
				const int aura_row = aura_frame / CORRUPTION_AURA_COLUMN_COUNT;
				const float aura_pulse = 1.0f + 0.035f * std::sin(
					g_MonsterAnimationElapsed * 7.0f);
				const float aura_width = std::abs(draw_width) *
					(slot.Type == MonsterType::BossCthulhu ? 1.18f : 1.05f) * aura_pulse;
				const float aura_height = draw_height *
					(slot.Type == MonsterType::BossCthulhu ? 1.72f : 1.48f) * aura_pulse;
				const DirectX::XMFLOAT2 aura_position{
					slot.Entity.GetPosition().x,
					slot.Entity.GetPosition().y - draw_height * 0.20f,
				};
				const DirectX::XMFLOAT2 aura_uv_offset{
					static_cast<float>(aura_column) /
						static_cast<float>(CORRUPTION_AURA_COLUMN_COUNT),
					static_cast<float>(aura_row) / 2.0f,
				};
				const DirectX::XMFLOAT2 aura_uv_scale{
					CORRUPTION_AURA_FRAME_WIDTH / 1536.0f,
					CORRUPTION_AURA_FRAME_HEIGHT / 1024.0f,
				};
				corruption_aura_instances.push_back({
					aura_position,
					{ aura_width, aura_height },
					0.0f,
					{ 0.66f, 0.24f, 1.0f, 0.82f * visual_alpha },
					aura_uv_offset,
					aura_uv_scale,
				});
				corruption_aura_glow_instances.push_back({
					aura_position,
					{ aura_width * 1.08f, aura_height * 1.06f },
					0.0f,
					{ 0.36f, 0.02f, 0.78f, 0.24f * visual_alpha },
					aura_uv_offset,
					aura_uv_scale,
				});
			}

			const float split_flash = GetBossSplitFlashAmount(slot);
			if (split_flash > 0.001f)
			{
				const float glow_scale = 1.06f + split_flash * 0.16f;
				boss_split_glow_instances.push_back({
					slot.Entity.GetPosition(),
					{ draw_width * glow_scale, draw_height * glow_scale },
					0.0f,
					{ 1.0f, 0.24f, 0.16f, split_flash * 0.72f },
					texcoord_offset,
					texcoord_scale,
				});
			}
		}
		else if (slot.Entity.IsActive())
		{
			const MonsterData& data = GetMonsterData(slot.Type);
			slot.Entity.Draw(
				texture_id,
				frame_x,
				frame_y,
				data.FrameWidth,
				data.FrameHeight,
				data.DrawSize.x * slot.DrawScale,
				data.DrawSize.y * slot.DrawScale,
				data.SourceFacesLeft);
		}
	}

	if (g_CorruptionAuraTextureID != TEXTURE_INVALID_ID &&
		!corruption_aura_instances.empty())
	{
		SpriteInstanced_DrawUnlit(
			g_CorruptionAuraTextureID,
			corruption_aura_instances.data(),
			static_cast<int>(corruption_aura_instances.size()));
		SpriteInstanced_DrawAdditiveUnlit(
			g_CorruptionAuraTextureID,
			corruption_aura_glow_instances.data(),
			static_cast<int>(corruption_aura_glow_instances.size()));
	}
	for (MonsterType type : MONSTER_TYPES)
	{
		std::vector<SpriteInstance>& afterimages =
			monster_afterimage_instances[MonsterTypeToIndex(type)];
		if (!afterimages.empty())
		{
			SpriteInstanced_DrawUnlit(
				GetMonsterTextureID(type),
				afterimages.data(),
				static_cast<int>(afterimages.size()));
		}
	}

	for (MonsterType type : MONSTER_TYPES)
	{
		std::vector<SpriteInstance>& instances =
			monster_instances[MonsterTypeToIndex(type)];
		if (!instances.empty())
		{
			SpriteInstanced_Draw(
				GetMonsterTextureID(type),
				instances.data(),
				static_cast<int>(instances.size()));
		}
	}
	if (!boss_split_glow_instances.empty())
	{
		SpriteInstanced_DrawAdditiveUnlit(
			GetMonsterTextureID(MonsterType::BossSlime),
			boss_split_glow_instances.data(),
			static_cast<int>(boss_split_glow_instances.size()));
	}
}

void DrawProjectiles()
{
	if (g_BossJellyTextureID != TEXTURE_INVALID_ID)
	{
		static std::vector<SpriteInstance> jelly_instances;
		static std::vector<SpriteInstance> jelly_glow_instances;
		jelly_instances.clear();
		jelly_glow_instances.clear();
		if (jelly_instances.capacity() < BOSS_JELLY_BULLET_MAX)
		{
			jelly_instances.reserve(BOSS_JELLY_BULLET_MAX);
			jelly_glow_instances.reserve(BOSS_JELLY_BULLET_MAX);
		}
		for (const BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (!bullet.IsActive || bullet.Style != BossProjectileStyle::Jelly)
			{
				continue;
			}
			jelly_glow_instances.push_back({
				bullet.Position,
				{ bullet.Size * 1.42f, bullet.Size * 1.42f },
				bullet.Rotation,
				bullet.IsPrimary ?
					DirectX::XMFLOAT4{ 0.34f, 1.0f, 0.10f, 0.32f } :
					DirectX::XMFLOAT4{ 0.62f, 1.0f, 0.16f, 0.25f },
			});
			jelly_instances.push_back({
				bullet.Position,
				{ bullet.Size, bullet.Size },
				bullet.Rotation,
				bullet.IsPrimary ?
					DirectX::XMFLOAT4{ 0.82f, 1.0f, 0.72f, 0.96f } :
					DirectX::XMFLOAT4{ 0.94f, 1.0f, 0.58f, 0.90f },
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				0.0f,
			});
		}
		if (!jelly_glow_instances.empty())
		{
			SpriteInstanced_DrawAdditiveUnlit(
				g_BossJellyTextureID,
				jelly_glow_instances.data(),
				static_cast<int>(jelly_glow_instances.size()));
		}
		if (!jelly_instances.empty())
		{
			SpriteInstanced_DrawOutlinedUnlit(
				g_BossJellyTextureID,
				jelly_instances.data(),
				static_cast<int>(jelly_instances.size()),
				{ 1.0f, 0.035f, 0.015f, 0.96f },
				1.0f);
		}
	}

	if (g_CorruptionProjectileTextureID != TEXTURE_INVALID_ID)
	{
		static std::vector<SpriteInstance> corruption_projectile_instances;
		static std::vector<SpriteInstance> corruption_projectile_glow_instances;
		corruption_projectile_instances.clear();
		corruption_projectile_glow_instances.clear();
		if (corruption_projectile_instances.capacity() < BOSS_JELLY_BULLET_MAX)
		{
			corruption_projectile_instances.reserve(BOSS_JELLY_BULLET_MAX);
			corruption_projectile_glow_instances.reserve(BOSS_JELLY_BULLET_MAX);
		}
		for (const BossJellyBullet& bullet : g_BossJellyBullets)
		{
			if (!bullet.IsActive || bullet.Style == BossProjectileStyle::Jelly)
			{
				continue;
			}
			const float pulse = 0.94f + 0.08f * std::sin(
				g_MonsterAnimationElapsed * 12.0f + bullet.Rotation * 2.0f);
			const DirectX::XMFLOAT4 glow_color =
				GetBossProjectileGlowColor(bullet.Style);
			const DirectX::XMFLOAT4 core_color =
				GetBossProjectileCoreColor(bullet.Style);
			corruption_projectile_glow_instances.push_back({
				bullet.Position,
				{ bullet.Size * 1.62f * pulse, bullet.Size * 1.62f * pulse },
				bullet.Rotation,
				glow_color,
			});
			corruption_projectile_instances.push_back({
				bullet.Position,
				{ bullet.Size, bullet.Size },
				bullet.Rotation,
				core_color,
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				0.0f,
			});
		}
		if (!corruption_projectile_glow_instances.empty())
		{
			SpriteInstanced_DrawAdditiveUnlit(
				g_CorruptionProjectileTextureID,
				corruption_projectile_glow_instances.data(),
				static_cast<int>(corruption_projectile_glow_instances.size()));
			SpriteInstanced_DrawOutlinedUnlit(
				g_CorruptionProjectileTextureID,
				corruption_projectile_instances.data(),
				static_cast<int>(corruption_projectile_instances.size()),
				{ 1.0f, 0.035f, 0.015f, 0.96f },
				1.0f);
		}
	}
	EnemyAttackPattern::DrawProjectiles();
}

int AppendPointLights(
	SpritePointLight* lights,
	int light_count,
	int capacity,
	const DirectX::XMFLOAT2& camera_position,
	const DirectX::XMFLOAT2& viewport_size)
{
	if (!lights || capacity <= 0)
	{
		return std::max(light_count, 0);
	}

	light_count = std::clamp(light_count, 0, capacity);
	struct LightCandidate
	{
		float DistanceSquared{ 0.0f };
		DirectX::XMFLOAT2 Position{};
		MonsterType Type{ DEFAULT_MONSTER_TYPE };
		int EnemyID{ 0 };
	};
	static std::vector<LightCandidate> candidates;
	candidates.clear();
	if (candidates.capacity() < ENEMY_MAX)
	{
		candidates.reserve(ENEMY_MAX);
	}

	const float visible_radius = std::sqrt(
		viewport_size.x * viewport_size.x +
		viewport_size.y * viewport_size.y) * 0.58f + 240.0f;
	const float visible_radius_squared = visible_radius * visible_radius;
	for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
	{
		const EnemySlot& slot = g_EnemySlots[enemy_id];
		if (!slot.Entity.IsAlive() ||
			!EnemyAttackPattern::IsTargetable(enemy_id))
		{
			continue;
		}

		const DirectX::XMFLOAT2 position = slot.Entity.GetPosition();
		const float dx = position.x - camera_position.x;
		const float dy = position.y - camera_position.y;
		const float distance_squared = dx * dx + dy * dy;
		if (distance_squared <= visible_radius_squared)
		{
			candidates.push_back({
				distance_squared, position, slot.Type, enemy_id });
		}
	}
	std::sort(candidates.begin(), candidates.end(),
		[](const LightCandidate& left, const LightCandidate& right)
		{
			return left.DistanceSquared < right.DistanceSquared;
		});

	const int append_count = std::min({
		capacity - light_count,
		static_cast<int>(candidates.size()) });
	const float stage_light_strength_scale =
		ProceduralMap_GetRound() == 1 ? 0.55f : 1.0f;
	for (int i = 0; i < append_count; ++i)
	{
		const LightCandidate& candidate = candidates[i];
		const MonsterLightStyle style = GetMonsterLightStyle(candidate.Type);
		const float flicker = 0.98f + 0.02f * std::sin(
			g_MonsterAnimationElapsed * 8.0f +
			static_cast<float>(candidate.EnemyID) * 1.73f);
		lights[light_count++] = {
			candidate.Position,
			style.Radius,
			style.Strength * stage_light_strength_scale * flicker,
			style.Color,
		};
	}

	static std::vector<int> jelly_light_candidates;
	jelly_light_candidates.clear();
	if (jelly_light_candidates.capacity() < BOSS_JELLY_BULLET_MAX)
	{
		jelly_light_candidates.reserve(BOSS_JELLY_BULLET_MAX);
	}
	for (int bullet_id = 0; bullet_id < BOSS_JELLY_BULLET_MAX; ++bullet_id)
	{
		const BossJellyBullet& bullet = g_BossJellyBullets[bullet_id];
		if (!bullet.IsActive)
		{
			continue;
		}
		jelly_light_candidates.push_back(bullet_id);
	}

	const int jelly_light_count = std::min({
		capacity - light_count,
		static_cast<int>(jelly_light_candidates.size()) });
	for (int i = 0; i < jelly_light_count; ++i)
	{
		const int bullet_id = jelly_light_candidates[i];
		const BossJellyBullet& bullet = g_BossJellyBullets[bullet_id];
		const float flicker = 0.94f + 0.06f * std::sin(
			g_MonsterAnimationElapsed * 10.0f +
			static_cast<float>(bullet_id) * 1.37f);
		const bool corruption = bullet.Style != BossProjectileStyle::Jelly;
		lights[light_count++] = {
			bullet.Position,
			corruption ? 175.0f : (bullet.IsPrimary ? 220.0f : 145.0f),
			(corruption ? 0.58f : (bullet.IsPrimary ? 0.68f : 0.46f)) * flicker,
			corruption ? GetBossProjectileLightColor(bullet.Style) : (bullet.IsPrimary ?
				DirectX::XMFLOAT3{ 0.30f, 1.0f, 0.08f } :
				DirectX::XMFLOAT3{ 0.58f, 1.0f, 0.12f }),
		};
	}
	return light_count;
}

void RegisterColliders()
{
	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		const EnemySlot& slot = g_EnemySlots[i];
		if (!slot.Entity.IsAlive() ||
			!EnemyAttackPattern::IsTargetable(i))
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			i,
			CollisionLayer::Enemy,
			CollisionLayer::Player | CollisionLayer::PlayerBullet,
			GetEnemyAimPosition(slot),
			slot.Entity.GetCollisionRadius());
	}
	for (int projectile_id = 0; projectile_id < BOSS_JELLY_BULLET_MAX; ++projectile_id)
	{
		const BossJellyBullet& bullet = g_BossJellyBullets[projectile_id];
		if (!bullet.IsActive || bullet.IsPrimary)
		{
			continue;
		}
		CollisionSystem_RegisterCircle(
			projectile_id,
			CollisionLayer::EnemyBullet,
			CollisionLayer::Player,
			bullet.Position,
			bullet.Radius);
	}
	EnemyAttackPattern::RegisterProjectileColliders(BOSS_JELLY_BULLET_MAX);
}

void HandleCollisionHits(cChainLightning& chain_lightning)
{
	for (int i = 0; i < CollisionSystem_GetHitCount(); ++i)
	{
		const cCollisionHit* hit = CollisionSystem_GetHit(i);
		if (!hit)
		{
			continue;
		}

		int enemy_id = -1;
		int projectile_id = PROJECTILE_INVALID_ID;
		if (!TryGetEnemyAndProjectile(*hit, enemy_id, projectile_id) ||
			!IsValidEnemyID(enemy_id) || !g_EnemySlots[enemy_id].Entity.IsAlive() ||
			!ProjectileSystem_IsActive(projectile_id))
		{
			continue;
		}

		if (!ProjectileSystem_TryRegisterTargetHit(projectile_id, enemy_id))
		{
			continue;
		}

		const cProjectile* projectile = ProjectileSystem_GetProjectile(projectile_id);
		if (!projectile)
		{
			continue;
		}

		const DirectX::XMFLOAT2 enemy_position =
			GetEnemyAimPosition(g_EnemySlots[enemy_id]);
		switch (projectile->HitBehavior)
		{
		case ProjectileHitBehavior::Area:
			ApplyAreaProjectileDamage(
				projectile->Position,
				projectile->AreaRadius,
				projectile->Damage,
				projectile->Velocity);
			break;

		case ProjectileHitBehavior::ChainLightning:
			ApplyProjectileDamage(
				enemy_id,
				projectile->Damage,
				projectile->Velocity,
				PLAYER_BULLET_KNOCKBACK_SPEED);
			chain_lightning.TryTrigger(
				enemy_id,
				enemy_position,
				projectile->Damage);
			break;

		case ProjectileHitBehavior::Pierce:
		case ProjectileHitBehavior::PersistentPierce:
			ApplyProjectileDamage(
				enemy_id,
				projectile->Damage,
				projectile->Velocity,
				PLAYER_BULLET_KNOCKBACK_SPEED);
			cGameEffectManager::GetInstance().Play(
				GameEffectType::VoidImplosion,
				enemy_position,
				0.72f);
			break;

		case ProjectileHitBehavior::Stop:
		default:
			ApplyProjectileDamage(
				enemy_id,
				projectile->Damage,
				projectile->Velocity,
				PLAYER_BULLET_KNOCKBACK_SPEED);
			break;
		}

		if (projectile->HitBehavior == ProjectileHitBehavior::PersistentPierce)
		{
			continue;
		}
		if (projectile->HitBehavior != ProjectileHitBehavior::Pierce ||
			projectile->TargetHitCount >= projectile->MaxTargetHits)
		{
			ProjectileSystem_Deactivate(projectile_id);
		}
	}
}

void Deactivate(int enemy_id)
{
	if (!IsValidEnemyID(enemy_id))
	{
		return;
	}
	g_EnemySlots[enemy_id].Entity.Deactivate();
	g_EnemySlots[enemy_id].RoomIndex = -1;
	EnemyAttackPattern::OnDeactivate(enemy_id);
}

bool ConsumeEnemyBullet(int projectile_id, float& out_damage)
{
	if (projectile_id >= BOSS_JELLY_BULLET_MAX)
	{
		return EnemyAttackPattern::ConsumeProjectile(
			projectile_id - BOSS_JELLY_BULLET_MAX,
			out_damage);
	}
	if (projectile_id < 0)
	{
		return false;
	}
	BossJellyBullet& bullet = g_BossJellyBullets[projectile_id];
	if (!bullet.IsActive)
	{
		return false;
	}
	out_damage = bullet.Damage;
	bullet.IsActive = false;
	return true;
}

bool IsActive(int enemy_id)
{
	return IsValidEnemyID(enemy_id) && g_EnemySlots[enemy_id].Entity.IsActive();
}
}
