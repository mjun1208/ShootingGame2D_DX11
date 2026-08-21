#include "game_enemy.h"

#include "blood.h"
#include "chain_lightning.h"
#include "collision.h"
#include "enemy.h"
#include "game_damage_text.h"
#include "game_effect.h"
#include "game_experience_gem.h"
#include "game_player.h"
#include "monster_data.h"
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
	constexpr int ENEMY_MAX = 512;
	constexpr float ENEMY_SPEED_VARIATION_RATIO = 0.18f;
	constexpr float PLAYER_BULLET_KNOCKBACK_SPEED = 100.0f;
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
	constexpr int MIN_ENEMIES_PER_WAVE = 4;
	constexpr int ENEMY_SPAWN_COUNT_MULTIPLIER = 16;
	constexpr MonsterType DEFAULT_MONSTER_TYPE = MonsterType::SkeletonBase;
	constexpr std::array<MonsterType, 6> MONSTER_TYPES{
		MonsterType::Slime,
		MonsterType::SkeletonBase,
		MonsterType::SkeletonMage,
		MonsterType::SkeletonRogue,
		MonsterType::SkeletonWarrior,
		MonsterType::Bat,
	};
	constexpr std::size_t MONSTER_TYPE_COUNT = MONSTER_TYPES.size();
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
	std::vector<PendingEnemySpawn> g_PendingEnemySpawns;
	std::vector<SpawnArrivalEffect> g_SpawnArrivalEffects;
	std::vector<BoneDropEffect> g_BoneDropEffects;
	std::array<int, MONSTER_TYPE_COUNT> g_MonsterTextureIDs{
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
	};
	int g_BoneDropTextureID = TEXTURE_INVALID_ID;
	int g_SpawnTelegraphTextureID = TEXTURE_INVALID_ID;
	std::array<DirectX::XMUINT2, MONSTER_TYPE_COUNT> g_MonsterTextureSizes{};
	int g_CurrentRoomIndex = -1;
	bool g_RoundCleared = false;
	float g_MonsterAnimationElapsed = 0.0f;
	float g_EnemyCollisionCellSize = cEnemy::RADIUS * 2.0f;
	std::uint32_t g_BoneRandomState = 0xB04E5EEDu;

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

	MonsterType SelectMonsterType(std::uint32_t hash)
	{
		float total_weight = 0.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			total_weight += MonsterDatabase::Get(type).SpawnWeight;
		}
		if (total_weight <= 0.0f) return DEFAULT_MONSTER_TYPE;
		const float selection =
			static_cast<float>(hash & 0x00ffffffu) /
			static_cast<float>(0x01000000u) * total_weight;
		float accumulated = 0.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			accumulated += MonsterDatabase::Get(type).SpawnWeight;
			if (selection < accumulated) return type;
		}
		return MonsterType::Bat;
	}

	int GetMonsterTextureID(MonsterType type)
	{
		return g_MonsterTextureIDs[MonsterTypeToIndex(type)];
	}

	void GetMonsterFrameRegion(
		MonsterType type,
		float animation_elapsed,
		int& frame_x,
		int& frame_y)
	{
		const MonsterData& data = MonsterDatabase::Get(type);
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
		const MonsterDeathEffect death_effect =
			MonsterDatabase::Get(type).DeathEffect;
		if (death_effect == MonsterDeathEffect::Slime)
		{
			cSlimeGoo::GetInstance().Spawn(
				position,
				impact_direction,
				was_killed ? 1.65f : 1.0f);
			return;
		}

		if (death_effect == MonsterDeathEffect::Bones)
		{
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
			if (!enemy.IsAlive())
			{
				continue;
			}

			const DirectX::XMFLOAT2 position = enemy.GetPosition();
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
				g_EnemySlots[enemy_id].Entity.GetPosition();
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
		const DirectX::XMFLOAT2 position_a = enemy_a.GetPosition();
		const DirectX::XMFLOAT2 position_b = enemy_b.GetPosition();
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
			const DirectX::XMFLOAT2 other = slot.Entity.GetPosition();
			const float minimum_distance =
				collision_radius + slot.Entity.GetCollisionRadius() + ENEMY_SEPARATION_PADDING;
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
		const int room_area = room.TileWidth * room.TileHeight;
		int encounter_total = std::clamp(room_area / 55 + room.Depth / 2, 8, 18);
		if (room.IsBossRoom)
		{
			encounter_total = std::min(encounter_total + 6, 24);
		}
		encounter_total = std::max(encounter_total, wave.WaveCount * MIN_ENEMIES_PER_WAVE);

		const int base_count = encounter_total / wave.WaveCount;
		const int remainder = encounter_total % wave.WaveCount;
		const int first_larger_wave = wave.WaveCount - remainder;
		const int wave_target = base_count +
			(wave.WaveIndex >= first_larger_wave ? 1 : 0);
		return wave_target * ENEMY_SPAWN_COUNT_MULTIPLIER;
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
		wave.WaveIndex = 0;
		wave.WaveCount = room->IsBossRoom ? 3 : 2 + static_cast<int>(encounter_hash & 1u);
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
			const MonsterData& monster_data = MonsterDatabase::Get(monster_type);
			DirectX::XMFLOAT2 spawn_position{};
			const float distance_requirement = attempt < maximum_attempts * 3 / 4 ?
				ENEMY_PLAYER_SPAWN_CLEARANCE : ENEMY_PLAYER_SPAWN_CLEARANCE * 0.65f;
			if (!ProceduralMap_TryGetRoomSpawnPosition(
				room_index,
				wave_sequence_salt + attempt + prepared_count * 37,
				player_position,
				distance_requirement,
				monster_data.CollisionRadius + ENEMY_WALL_PADDING,
				spawn_position) ||
				!IsFarEnoughFromEnemies(
					spawn_position,
					monster_data.CollisionRadius,
					spawned_positions))
			{
				continue;
			}

			const float speed_amount =
				static_cast<float>(spawn_hash & 0xffffu) / 65535.0f;
			const float speed_variation =
				1.0f + (speed_amount * 2.0f - 1.0f) * ENEMY_SPEED_VARIATION_RATIO;
			const float speed = monster_data.MoveSpeed * speed_variation +
				(room->IsBossRoom ? 18.0f : 0.0f);

			g_PendingEnemySpawns.push_back({
				spawn_position,
				speed,
				0.0f,
				monster_type,
				room_index,
			});
			spawned_positions.push_back({
				spawn_position,
				monster_data.CollisionRadius,
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

		const MonsterType monster_type = SelectMonsterType(Hash32(
			ProceduralMap_GetSeed() ^
			static_cast<std::uint32_t>(room_index + 1) * 0x9e3779b9u ^
			static_cast<std::uint32_t>(wave.WaveIndex + 1) * 0x85ebca6bu));
		const MonsterData& monster_data = MonsterDatabase::Get(monster_type);
		const float wall_clearance =
			monster_data.CollisionRadius + ENEMY_WALL_PADDING;
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
			(room->IsBossRoom ? 18.0f : 0.0f);
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
			const MonsterData& monster_data = MonsterDatabase::Get(pending.Type);
			slot.Entity.Spawn(
				pending.Position,
				pending.Speed,
				monster_data.MaxHitPoint,
				monster_data.CollisionRadius);
			slot.Type = pending.Type;
			slot.RoomIndex = pending.RoomIndex;
			g_SpawnArrivalEffects.push_back({ pending.Position, 0.0f });
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

		cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
		const DirectX::XMFLOAT2 enemy_position = enemy.GetPosition();
		GameDamageText::Spawn(damage, enemy_position);
		enemy.ApplyKnockback(knockback_direction, knockback_speed);
		enemy.ApplyDamage(damage);
		PlayDamageReaction(
			g_EnemySlots[enemy_id].Type,
			enemy_position,
			knockback_direction,
			!enemy.IsAlive());
		if (!enemy.IsAlive())
		{
			cGameEffectManager::GetInstance().PlayEnemyDefeat(enemy_position);
			GameExperienceGem::Spawn(
				enemy_position,
				MonsterDatabase::Get(g_EnemySlots[enemy_id].Type).ExperienceDrop);
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

		cGameEffectManager::GetInstance().Play(
			GameEffectType::WarmExplosion,
			center,
			1.35f);

		for (int enemy_id = 0; enemy_id < ENEMY_MAX; ++enemy_id)
		{
			const cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
			if (!enemy.IsAlive())
			{
				continue;
			}
			const float hit_radius = radius + enemy.GetCollisionRadius();
			const float hit_radius_sq = hit_radius * hit_radius;

			const DirectX::XMFLOAT2 enemy_position = enemy.GetPosition();
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
	MonsterDatabase::Load("asset/data/monsters.json");
	for (MonsterType type : MONSTER_TYPES)
	{
		const std::size_t type_index = MonsterTypeToIndex(type);
		g_MonsterTextureIDs[type_index] = Texture_Load(
			MonsterDatabase::Get(type).TexturePath.c_str(), false);
		g_MonsterTextureSizes[type_index] =
			Texture_GetSize(g_MonsterTextureIDs[type_index]);
	}
	g_EnemyCollisionCellSize = 1.0f;
	for (MonsterType type : MONSTER_TYPES)
	{
		g_EnemyCollisionCellSize = std::max(
			g_EnemyCollisionCellSize,
			MonsterDatabase::Get(type).CollisionRadius * 2.0f);
	}
	g_BoneDropTextureID = Texture_Load(
		L"asset/monster/04_skeleton/04_skeleton_white_bones.png", false);
	g_SpawnTelegraphTextureID = Texture_Load(
		L"asset/texture/vfx/spawn_magic_circle/magic_circle.png", false);
	ResetDungeon();
}

void Finalize()
{
	ResetDungeon();
	g_RoomWaves.clear();
	for (int& texture_id : g_MonsterTextureIDs)
	{
		Texture_Release(texture_id);
		texture_id = TEXTURE_INVALID_ID;
	}
	Texture_Release(g_BoneDropTextureID);
	g_BoneDropTextureID = TEXTURE_INVALID_ID;
	Texture_Release(g_SpawnTelegraphTextureID);
	g_SpawnTelegraphTextureID = TEXTURE_INVALID_ID;
}

void ResetDungeon()
{
	ProceduralMap_ClearEncounterLock();
	g_RoundCleared = false;
	g_MonsterAnimationElapsed = 0.0f;
	g_PendingEnemySpawns.clear();
	g_SpawnArrivalEffects.clear();
	g_BoneDropEffects.clear();
	g_BoneRandomState = 0xB04E5EEDu ^ ProceduralMap_GetSeed();
	for (EnemySlot& slot : g_EnemySlots)
	{
		slot.Entity = cEnemy{};
		slot.Type = DEFAULT_MONSTER_TYPE;
		slot.RoomIndex = -1;
	}

	g_RoomWaves.assign(ProceduralMap_GetRoomCount(), RoomWaveRuntime{});
	const int start_room = ProceduralMap_GetStartRoomIndex();
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
	if (room_index >= 0 && room_index != g_CurrentRoomIndex)
	{
		AbandonOtherActiveRooms(room_index);
		g_CurrentRoomIndex = room_index;
	}
	if (room_index >= 0)
	{
		BeginRoomEncounter(room_index, player_position);
	}

	for (EnemySlot& slot : g_EnemySlots)
	{
		slot.Entity.Update(delta_time, player_position);
		if (!slot.Entity.IsActive())
		{
			slot.RoomIndex = -1;
		}
	}
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
		if (!enemy.IsAlive())
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

		const DirectX::XMFLOAT2 position = enemy.GetPosition();
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
		!g_EnemySlots[enemy_id].Entity.IsAlive())
	{
		return false;
	}

	cEnemy& enemy = g_EnemySlots[enemy_id].Entity;
	const DirectX::XMFLOAT2 enemy_position = enemy.GetPosition();
	GameDamageText::Spawn(damage, enemy_position);
	enemy.ApplyDamage(damage);
	PlayDamageReaction(
		g_EnemySlots[enemy_id].Type,
		enemy_position,
		{ 0.0f, 0.0f },
		!enemy.IsAlive());
	if (!enemy.IsAlive())
	{
		cGameEffectManager::GetInstance().PlayEnemyDefeat(enemy_position);
		GameExperienceGem::Spawn(
			enemy_position,
			MonsterDatabase::Get(g_EnemySlots[enemy_id].Type).ExperienceDrop);
	}
	return true;
}

bool IsRoundCleared()
{
	return g_RoundCleared;
}

void DrawSpawnTelegraphs()
{
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
		const float size = SPAWN_TELEGRAPH_SIZE * (0.96f + completion_flash * 0.04f);
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
		const float size = SPAWN_TELEGRAPH_SIZE +
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

	SpriteInstanced_DrawAdditive(
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
		SpriteInstanced_Draw(
			g_BoneDropTextureID,
			bone_instances.data(),
			static_cast<int>(bone_instances.size()));
	}

	static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_instances;
	for (std::vector<SpriteInstance>& instances : monster_instances)
	{
		if (instances.capacity() < ENEMY_MAX)
		{
			instances.reserve(ENEMY_MAX);
		}
		instances.clear();
	}

	for (const EnemySlot& slot : g_EnemySlots)
	{
		int frame_x = 0;
		int frame_y = 0;
		GetMonsterFrameRegion(
			slot.Type, g_MonsterAnimationElapsed, frame_x, frame_y);
		const int texture_id = GetMonsterTextureID(slot.Type);
		if (slot.Entity.IsAlive())
		{
			std::vector<SpriteInstance>& instances =
				monster_instances[MonsterTypeToIndex(slot.Type)];
			const MonsterData& data = MonsterDatabase::Get(slot.Type);
			const DirectX::XMUINT2 texture_size =
				g_MonsterTextureSizes[MonsterTypeToIndex(slot.Type)];
			const bool flip_horizontal =
				slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
			instances.push_back({
				slot.Entity.GetPosition(),
				{
					flip_horizontal ? -data.DrawSize.x : data.DrawSize.x,
					data.DrawSize.y,
				},
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f },
				{
					static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
					static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
				},
				{
					static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x),
					static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y),
				},
			});
		}
		else if (slot.Entity.IsActive())
		{
			const MonsterData& data = MonsterDatabase::Get(slot.Type);
			slot.Entity.Draw(
				texture_id,
				frame_x,
				frame_y,
				data.FrameWidth,
				data.FrameHeight,
				data.DrawSize.x,
				data.DrawSize.y,
				data.SourceFacesLeft);
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
}

void DrawMapMarkers(
	const DirectX::XMFLOAT2& map_origin,
	float world_scale,
	bool expanded)
{
	static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_markers;
	for (std::vector<SpriteInstance>& markers : monster_markers)
	{
		if (markers.capacity() < ENEMY_MAX)
		{
			markers.reserve(ENEMY_MAX);
		}
		markers.clear();
	}
	const float marker_size = expanded ? 13.0f : 8.0f;
	for (const EnemySlot& slot : g_EnemySlots)
	{
		if (!slot.Entity.IsActive())
		{
			continue;
		}
		std::vector<SpriteInstance>& markers =
			monster_markers[MonsterTypeToIndex(slot.Type)];
		const DirectX::XMFLOAT2 enemy_position = slot.Entity.GetPosition();
		const MonsterData& data = MonsterDatabase::Get(slot.Type);
		const DirectX::XMUINT2 texture_size =
			g_MonsterTextureSizes[MonsterTypeToIndex(slot.Type)];
		markers.push_back({
			{
				map_origin.x + enemy_position.x * world_scale,
				map_origin.y + enemy_position.y * world_scale,
			},
			{ marker_size, marker_size },
			0.0f,
			{ 1.0f, 0.34f, 0.24f, 1.0f },
			{ 0.0f, 0.0f },
			{
				static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x),
				static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y),
			},
		});
	}
	for (MonsterType type : MONSTER_TYPES)
	{
		std::vector<SpriteInstance>& markers =
			monster_markers[MonsterTypeToIndex(type)];
		if (!markers.empty())
		{
			SpriteInstanced_Draw(
				GetMonsterTextureID(type),
				markers.data(),
				static_cast<int>(markers.size()));
		}
	}
}

void RegisterColliders()
{
	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		g_EnemySlots[i].Entity.RegisterCollider(i);
	}
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
			g_EnemySlots[enemy_id].Entity.GetPosition();
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
}

bool IsActive(int enemy_id)
{
	return IsValidEnemyID(enemy_id) && g_EnemySlots[enemy_id].Entity.IsActive();
}
}
