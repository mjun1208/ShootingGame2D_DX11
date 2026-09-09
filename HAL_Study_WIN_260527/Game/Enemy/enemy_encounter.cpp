#include "game_enemy_internal.h"
#include "Constants/enemy_constants.h"

namespace
{

	namespace EnemyTuning::Movement
	{
		constexpr float SpeedVariationRatio = 0.18f;
	} // namespace EnemyTuning::Movement

	namespace EnemyTuning::SlimeRush
	{
		constexpr int EnemyCount = 50;
		constexpr int WaveCount = 2;
	} // namespace EnemyTuning::SlimeRush

	namespace EnemyTuning::Spawn
	{
		constexpr float Clearance = 330.0f;
	} // namespace EnemyTuning::Spawn

} // namespace

namespace GameEnemy::Internal
{
	// 방 진입부터 웨이브 생성, 방 클리어까지의 흐름을 관리한다.
	int GetActiveCountInRoom(int room_index)
	{
		int count = 0;
		for (const EnemySlot& slot : g_EnemySystem.EnemySlots)
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
		return state == RoomWaveState::Waiting || state == RoomWaveState::Telegraphing ||
		       state == RoomWaveState::Active;
	}

	void CompleteRoomEncounter(int room_index)
	{
		if (room_index >= 0 && room_index < static_cast<int>(g_EnemySystem.RoomWaves.size()))
		{
			RoomWaveRuntime& wave = g_EnemySystem.RoomWaves[room_index];
			wave.State = RoomWaveState::Cleared;
			wave.DelayRemaining = 0.0f;
			wave.SpawnRetryCount = 0;
			if (room_index == ProceduralMap_GetFinalEncounterRoomIndex())
			{
				g_EnemySystem.RoundCleared = true;
			}
			GameExperienceGem::AttractAllInRoom(room_index);
		}
		ClearPendingEnemySpawns(room_index);
		ProceduralMap_ClearEncounterLock(room_index);
	}

	void AbortRoomEncounter(int room_index)
	{
		if (room_index >= 0 && room_index < static_cast<int>(g_EnemySystem.RoomWaves.size()))
		{
			g_EnemySystem.RoomWaves[room_index] = RoomWaveRuntime{};
		}
		ClearPendingEnemySpawns(room_index);
		ProceduralMap_ClearEncounterLock(room_index);
	}

	void AbandonOtherActiveRooms(int entered_room_index)
	{
		for (EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			if (slot.Entity.IsActive() && slot.RoomIndex != entered_room_index)
			{
				slot.Entity.Deactivate();
				slot.RoomIndex = -1;
			}
		}
		for (int room_index = 0; room_index < static_cast<int>(g_EnemySystem.RoomWaves.size()); ++room_index)
		{
			RoomWaveRuntime& wave = g_EnemySystem.RoomWaves[room_index];
			if (room_index != entered_room_index && IsEncounterInProgress(wave.State))
			{
				ProceduralMap_ClearEncounterLock(room_index);
				wave = RoomWaveRuntime{};
			}
		}
		std::erase_if(g_EnemySystem.PendingEnemySpawns,
		              [entered_room_index](const PendingEnemySpawn& spawn)
		              {
			              return spawn.RoomIndex != entered_room_index;
		              });
	}

	int GetWaveTargetCount(const ProceduralMapRoom& room, const RoomWaveRuntime& wave)
	{
		if (IsSlimeRushRoom(room))
		{
			return EnemyTuning::SlimeRush::EnemyCount;
		}
		const int encounter_total = std::max(wave.TotalEnemyCount, wave.WaveCount);
		const int base_count = encounter_total / wave.WaveCount;
		const int remainder = encounter_total % wave.WaveCount;
		const int first_larger_wave = wave.WaveCount - remainder;
		const int wave_target = base_count + (wave.WaveIndex >= first_larger_wave ? 1 : 0);
		return wave_target;
	}

	void BeginRoomEncounter(int room_index, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float EntryInset = 72.0f;
		if (room_index < 0 || room_index >= static_cast<int>(g_EnemySystem.RoomWaves.size()) ||
		    g_EnemySystem.RoomWaves[room_index].State != RoomWaveState::Dormant)
		{
			return;
		}
		RoomWaveRuntime& wave = g_EnemySystem.RoomWaves[room_index];
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
		if (room_index == ProceduralMap_GetStartRoomIndex() && room_index != ProceduralMap_GetFinalEncounterRoomIndex())
		{
			CompleteRoomEncounter(room_index);
			return;
		}
		if (player_position.x < room->WorldMin.x + EntryInset || player_position.x > room->WorldMax.x - EntryInset ||
		    player_position.y < room->WorldMin.y + EntryInset || player_position.y > room->WorldMax.y - EntryInset)
		{
			return;
		}
		// 방 전투를 시작하며 웨이브 인덱스와 개수를 초기화한다.
		const RoundEncounterData& encounter = GetMapData().GetRoundEncounter(ProceduralMap_GetRound());
		wave.WaveIndex = 0;
		wave.WaveCount = IsSlimeRushRoom(*room)
		                     ? EnemyTuning::SlimeRush::WaveCount
		                     : (IsBossEncounterRoom(room_index)
		                            ? 1
		                            : (room->IsLargeRoom ? encounter.LargeRoomWaveCount
		                                                 : RandomInt(encounter.MinWaveCount, encounter.MaxWaveCount)));
		wave.TotalEnemyCount = RandomInt(encounter.MinEnemiesPerRoom, encounter.MaxEnemiesPerRoom);
		if (room->IsLargeRoom)
		{
			wave.TotalEnemyCount += encounter.LargeRoomEnemyBonus;
		}
		wave.TotalEnemyCount = std::max(wave.TotalEnemyCount, wave.WaveCount);
		wave.SpawnRetryCount = 0;
		wave.DelayRemaining = 0.0f;
		if (!ProceduralMap_LockEncounterRoom(room_index))
		{
			AbortRoomEncounter(room_index);
			return;
		}
		wave.State = RoomWaveState::Waiting;
	}

	int PrepareSlimeRushWave(int room_index, const ProceduralMapRoom& room, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr int RoomTileInset = 2;
		if (GetFreeEnemySlotCount() < EnemyTuning::SlimeRush::EnemyCount || room.TileWidth <= RoomTileInset * 2 ||
		    room.TileHeight <= RoomTileInset * 2)
		{
			return 0;
		}
		const MonsterData& slime_data = GetMonsterData(MonsterType::Slime);
		const MonsterMapCollisionShape map_collision = GetMonsterMapCollisionShape(MonsterType::Slime);
		const float wall_clearance = map_collision.Radius + EnemyConstants::Collision::WallPadding;
		const float tile_width = (room.WorldMax.x - room.WorldMin.x) / room.TileWidth;
		const float tile_height = (room.WorldMax.y - room.WorldMin.y) / room.TileHeight;
		const float minimum_player_distance_squared = EnemyTuning::Spawn::Clearance * EnemyTuning::Spawn::Clearance;
		std::vector<DirectX::XMFLOAT2> candidates;
		candidates.reserve((room.TileWidth - RoomTileInset * 2) * (room.TileHeight - RoomTileInset * 2) / 2);
		for (int tile_y = RoomTileInset; tile_y < room.TileHeight - RoomTileInset; ++tile_y)
		{
			for (int tile_x = RoomTileInset; tile_x < room.TileWidth - RoomTileInset; ++tile_x)
			{
				// 슬라임끼리 겹치지 않도록 한 칸씩 건너뛰어 배치한다.
				if ((tile_x + tile_y) % 2 != 0)
				{
					continue;
				}
				const DirectX::XMFLOAT2 position{
					room.WorldMin.x + (static_cast<float>(tile_x) + 0.5f) * tile_width,
					room.WorldMin.y + (static_cast<float>(tile_y) + 0.5f) * tile_height,
				};
				if (DistanceSquared(position, player_position) < minimum_player_distance_squared ||
				    !ProceduralMap_IsCircleWalkable(position, wall_clearance))
				{
					continue;
				}
				candidates.push_back(position);
			}
		}
		if (static_cast<int>(candidates.size()) < EnemyTuning::SlimeRush::EnemyCount)
		{
			return 0;
		}
		ClearPendingEnemySpawns(room_index);
		for (int spawn_index = 0; spawn_index < EnemyTuning::SlimeRush::EnemyCount; ++spawn_index)
		{
			// 후보 전체에 고르게 분산하여 특정 위치에 몰리지 않게 한다.
			const std::size_t candidate_index =
			    static_cast<std::size_t>(spawn_index) * candidates.size() / EnemyTuning::SlimeRush::EnemyCount;
			const float speed_variation = 1.0f + RandomSigned() * EnemyTuning::Movement::SpeedVariationRatio;
			g_EnemySystem.PendingEnemySpawns.push_back({
			    candidates[candidate_index],
			    slime_data.MoveSpeed * speed_variation + 18.0f,
			    0.0f,
			    MonsterType::Slime,
			    room_index,
			});
		}
		return EnemyTuning::SlimeRush::EnemyCount;
	}

	int PrepareCurrentWave(int room_index, RoomWaveRuntime& wave, const DirectX::XMFLOAT2& player_position)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room || wave.WaveCount <= 0 || wave.WaveIndex < 0 || wave.WaveIndex >= wave.WaveCount)
		{
			return 0;
		}
		// 적을 바로 생성하지 않고 이번 웨이브의 생성 위치를 예약한다.
		ClearPendingEnemySpawns(room_index);
		if (IsSlimeRushRoom(*room))
		{
			return PrepareSlimeRushWave(room_index, *room, player_position);
		}
		if (IsBossEncounterRoom(room_index))
		{
			if (GetFreeEnemySlotCount() <= 0)
			{
				return 0;
			}
			const MonsterType boss_type = GetBossTypeForRound(ProceduralMap_GetRound());
			const MonsterData& boss_data = GetMonsterData(boss_type);
			const MonsterMapCollisionShape map_collision = GetMonsterMapCollisionShape(boss_type);
			const float wall_clearance = map_collision.Radius + EnemyConstants::Collision::WallPadding;
			DirectX::XMFLOAT2 spawn_position = room->Center;
			if (!ProceduralMap_IsCircleWalkable(spawn_position, wall_clearance))
			{
				if (!ProceduralMap_TryGetRoomSpawnPosition(room_index, player_position, EnemyTuning::Spawn::Clearance,
				                                           wall_clearance, spawn_position))
				{
					return 0;
				}
			}
			g_EnemySystem.PendingEnemySpawns.push_back({
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
		for (int attempt = 0; attempt < maximum_attempts && prepared_count < target_count; ++attempt)
		{
			const MonsterType monster_type = SelectMonsterType();
			const MonsterData& monster_data = GetMonsterData(monster_type);
			const MonsterMapCollisionShape map_collision = GetMonsterMapCollisionShape(monster_type);
			DirectX::XMFLOAT2 spawn_position{};
			const float distance_requirement = attempt < maximum_attempts * 3 / 4
			                                       ? EnemyTuning::Spawn::Clearance
			                                       : EnemyTuning::Spawn::Clearance * 0.65f;
			if (!ProceduralMap_TryGetRoomSpawnPosition(room_index, player_position, distance_requirement,
			                                           map_collision.Radius + EnemyConstants::Collision::WallPadding,
			                                           spawn_position) ||
			    !IsFarEnoughFromEnemies(spawn_position, map_collision.Radius, spawned_positions))
			{
				continue;
			}
			const float speed_variation = 1.0f + RandomSigned() * EnemyTuning::Movement::SpeedVariationRatio;
			const float speed = monster_data.MoveSpeed * speed_variation + (room->IsLargeRoom ? 18.0f : 0.0f);
			g_EnemySystem.PendingEnemySpawns.push_back({
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

	bool PrepareFallbackEnemy(int room_index, const RoomWaveRuntime& wave, const DirectX::XMFLOAT2& player_position)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room || GetFreeEnemySlotCount() <= 0)
		{
			return false;
		}
		if (IsSlimeRushRoom(*room))
		{
			return PrepareSlimeRushWave(room_index, *room, player_position) == EnemyTuning::SlimeRush::EnemyCount;
		}
		const MonsterType monster_type =
		    IsBossEncounterRoom(room_index) ? GetBossTypeForRound(ProceduralMap_GetRound()) : SelectMonsterType();
		const MonsterData& monster_data = GetMonsterData(monster_type);
		const MonsterMapCollisionShape map_collision = GetMonsterMapCollisionShape(monster_type);
		const float wall_clearance = map_collision.Radius + EnemyConstants::Collision::WallPadding;
		DirectX::XMFLOAT2 spawn_position{};
		if (!ProceduralMap_TryGetRoomSpawnPosition(room_index, player_position, EnemyTuning::Spawn::Clearance * 0.35f,
		                                           wall_clearance, spawn_position))
		{
			spawn_position = room->Center;
			if (!ProceduralMap_IsCircleWalkable(spawn_position, wall_clearance))
			{
				return false;
			}
		}
		const float speed = monster_data.MoveSpeed + (room->IsLargeRoom ? 18.0f : 0.0f);
		ClearPendingEnemySpawns(room_index);
		g_EnemySystem.PendingEnemySpawns.push_back({
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
		// 예약한 위치에 이 방의 적을 생성하고 생성된 수를 반환한다.
		int spawned_count = 0;
		for (const PendingEnemySpawn& pending : g_EnemySystem.PendingEnemySpawns)
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
			EnemySlot& slot = g_EnemySystem.EnemySlots[slot_index];
			const MonsterData& monster_data = GetMonsterData(pending.Type);
			const MonsterMapCollisionShape map_collision = GetMonsterMapCollisionShape(pending.Type);
			const DirectX::XMFLOAT2 entity_position{
				pending.Position.x - map_collision.Offset.x,
				pending.Position.y - map_collision.Offset.y,
			};
			cEnemy::SpawnSettings spawn_settings{};
			spawn_settings.Speed = pending.Speed;
			spawn_settings.MaxHitPoint = monster_data.MaxHitPoint;
			spawn_settings.CollisionRadius = monster_data.CollisionRadius;
			spawn_settings.MapCollisionOffset = map_collision.Offset;
			spawn_settings.MapCollisionRadius = map_collision.Radius;
			slot.Entity.Spawn(entity_position, spawn_settings);
			slot.Type = pending.Type;
			slot.RoomIndex = pending.RoomIndex;
			slot.DrawScale = 1.0f;
			slot.ExperienceScale = 1.0f;
			slot.BossFireScaleElapsed = EnemyConstants::BossJelly::FireScaleDuration;
			slot.BossSplitScaleElapsed = EnemyConstants::BossBody::SplitScaleDuration;
			EnemyAttackPattern::OnSpawn(slot_index, pending.Type);
			g_EnemySystem.SpawnArrivalEffects.push_back({
			    pending.Position,
			    0.0f,
			    GetSpawnTelegraphSize(pending.Type),
			});
			++spawned_count;
		}
		ClearPendingEnemySpawns(room_index);
		return spawned_count;
	}

	void UpdateRoomEncounter(int room_index, float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr int MaxSpawnRetries = 20;
		static constexpr float RetryDelay = 0.15f;
		static constexpr float BetweenWaveDelay = 0.65f;
		if (room_index < 0 || room_index >= static_cast<int>(g_EnemySystem.RoomWaves.size()))
		{
			return;
		}
		// Waiting -> Telegraphing -> Active 순서로 각 웨이브가 진행된다.
		RoomWaveRuntime& wave = g_EnemySystem.RoomWaves[room_index];
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
			wave.DelayRemaining = RetryDelay;
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
				wave.DelayRemaining = EnemyConstants::Spawn::TelegraphDuration;
				return;
			}
			++wave.SpawnRetryCount;
			if (wave.SpawnRetryCount >= MaxSpawnRetries)
			{
				if (PrepareFallbackEnemy(room_index, wave, player_position))
				{
					wave.State = RoomWaveState::Telegraphing;
					wave.SpawnRetryCount = 0;
					wave.DelayRemaining = EnemyConstants::Spawn::TelegraphDuration;
					return;
				}
				// 생성 가능한 위치가 없으면 예약을 유지한 채 잠시 후 다시 시도한다.
				wave.SpawnRetryCount = 0;
				wave.DelayRemaining = 1.0f;
				return;
			}
			wave.DelayRemaining = RetryDelay;
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
			wave.DelayRemaining = BetweenWaveDelay;
		}
	}
} // namespace GameEnemy::Internal
