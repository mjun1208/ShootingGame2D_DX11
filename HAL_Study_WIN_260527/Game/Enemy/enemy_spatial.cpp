#include "game_enemy_internal.h"
#include "game_enemy.h"
#include "Constants/enemy_constants.h"

namespace
{
	namespace EnemyTuning::Collision
	{
		constexpr int GridInvalidIndex = -1;
	} // namespace EnemyTuning::Collision

} // namespace

namespace GameEnemy::Internal
{
	// 적 슬롯과 공간 해시를 관리하고 적들이 겹치지 않도록 밀어낸다.
	int FindFreeEnemySlot()
	{
		for (int i = 0; i < GameEnemy::ENEMY_CAPACITY; ++i)
		{
			if (!g_EnemySystem.EnemySlots[i].Entity.IsActive())
			{
				return i;
			}
		}
		return -1;
	}

	int GetFreeEnemySlotCount()
	{
		int count = 0;
		for (const EnemySlot& slot : g_EnemySystem.EnemySlots)
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
		std::erase_if(g_EnemySystem.PendingEnemySpawns,
		              [room_index](const PendingEnemySpawn& spawn)
		              {
			              return spawn.RoomIndex == room_index;
		              });
	}

	void BuildEnemyCollisionGrid()
	{
		std::fill_n(g_EnemySystem.CollisionGrid.BucketHeads, EnemyConstants::Collision::GridBucketCount,
		            EnemyTuning::Collision::GridInvalidIndex);
		std::fill_n(g_EnemySystem.CollisionGrid.NextEnemy, GameEnemy::ENEMY_CAPACITY,
		            EnemyTuning::Collision::GridInvalidIndex);
		g_EnemySystem.CollisionGrid.HasAliveEnemy = false;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			if (!IsAliveAndTargetable(enemy_id))
			{
				continue;
			}
			const DirectX::XMFLOAT2 position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			const int cell_x = WorldToGridCell(position.x, g_EnemySystem.EnemyCollisionCellSize);
			const int cell_y = WorldToGridCell(position.y, g_EnemySystem.EnemyCollisionCellSize);
			if (!g_EnemySystem.CollisionGrid.HasAliveEnemy)
			{
				g_EnemySystem.CollisionGrid.MinCellX = cell_x;
				g_EnemySystem.CollisionGrid.MaxCellX = cell_x;
				g_EnemySystem.CollisionGrid.MinCellY = cell_y;
				g_EnemySystem.CollisionGrid.MaxCellY = cell_y;
				g_EnemySystem.CollisionGrid.HasAliveEnemy = true;
			}
			else
			{
				g_EnemySystem.CollisionGrid.MinCellX = std::min(g_EnemySystem.CollisionGrid.MinCellX, cell_x);
				g_EnemySystem.CollisionGrid.MaxCellX = std::max(g_EnemySystem.CollisionGrid.MaxCellX, cell_x);
				g_EnemySystem.CollisionGrid.MinCellY = std::min(g_EnemySystem.CollisionGrid.MinCellY, cell_y);
				g_EnemySystem.CollisionGrid.MaxCellY = std::max(g_EnemySystem.CollisionGrid.MaxCellY, cell_y);
			}
			const int bucket = SpatialHashIndex(cell_x, cell_y, EnemyConstants::Collision::GridBucketCount);
			g_EnemySystem.CollisionGrid.CellX[enemy_id] = cell_x;
			g_EnemySystem.CollisionGrid.CellY[enemy_id] = cell_y;
			g_EnemySystem.CollisionGrid.NextEnemy[enemy_id] = g_EnemySystem.CollisionGrid.BucketHeads[bucket];
			g_EnemySystem.CollisionGrid.BucketHeads[bucket] = enemy_id;
		}
	}

	void CheckNearestEnemyInCell(int cell_x, int cell_y, const DirectX::XMFLOAT2& origin, bool& found,
	                             float& best_distance_sq, DirectX::XMFLOAT2& best_position)
	{
		const int bucket = SpatialHashIndex(cell_x, cell_y, EnemyConstants::Collision::GridBucketCount);
		for (int enemy_id = g_EnemySystem.CollisionGrid.BucketHeads[bucket];
		     enemy_id != EnemyTuning::Collision::GridInvalidIndex;
		     enemy_id = g_EnemySystem.CollisionGrid.NextEnemy[enemy_id])
		{
			if (g_EnemySystem.CollisionGrid.CellX[enemy_id] != cell_x ||
			    g_EnemySystem.CollisionGrid.CellY[enemy_id] != cell_y ||
			    !g_EnemySystem.EnemySlots[enemy_id].Entity.IsAlive())
			{
				continue;
			}
			const DirectX::XMFLOAT2 position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			const float distance_sq = DistanceSquared(position, origin);
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
		cEnemy& enemy_a = g_EnemySystem.EnemySlots[enemy_id_a].Entity;
		cEnemy& enemy_b = g_EnemySystem.EnemySlots[enemy_id_b].Entity;
		const DirectX::XMFLOAT2 position_a = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id_a]);
		const DirectX::XMFLOAT2 position_b = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id_b]);
		float delta_x = position_b.x - position_a.x;
		float delta_y = position_b.y - position_a.y;
		const float distance_sq = DistanceSquared(position_a, position_b);
		const float collision_distance = enemy_a.GetCollisionRadius() + enemy_b.GetCollisionRadius();
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
			// 위치가 완전히 같으면 적 ID에 따라 일정한 방향으로 분리한다.
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
		static constexpr int SolverIterations = 4;
		for (int iteration = 0; iteration < SolverIterations; ++iteration)
		{
			BuildEnemyCollisionGrid();
			bool found_overlap = false;
			for (int i = 0; i < GameEnemy::ENEMY_CAPACITY; ++i)
			{
				if (!g_EnemySystem.EnemySlots[i].Entity.IsAlive())
				{
					continue;
				}
				const int center_cell_x = g_EnemySystem.CollisionGrid.CellX[i];
				const int center_cell_y = g_EnemySystem.CollisionGrid.CellY[i];
				for (int offset_y = -1; offset_y <= 1; ++offset_y)
				{
					for (int offset_x = -1; offset_x <= 1; ++offset_x)
					{
						const int cell_x = center_cell_x + offset_x;
						const int cell_y = center_cell_y + offset_y;
						const int bucket = SpatialHashIndex(cell_x, cell_y, EnemyConstants::Collision::GridBucketCount);
						for (int j = g_EnemySystem.CollisionGrid.BucketHeads[bucket];
						     j != EnemyTuning::Collision::GridInvalidIndex;
						     j = g_EnemySystem.CollisionGrid.NextEnemy[j])
						{
							if (j <= i || g_EnemySystem.CollisionGrid.CellX[j] != cell_x ||
							    g_EnemySystem.CollisionGrid.CellY[j] != cell_y)
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

	bool IsFarEnoughFromEnemies(const DirectX::XMFLOAT2& position, float collision_radius,
	                            const std::vector<SpawnPlacement>& new_positions)
	{
		static constexpr float SeparationPadding = 4.0f;
		for (const SpawnPlacement& other : new_positions)
		{
			const float minimum_distance = collision_radius + other.CollisionRadius + SeparationPadding;
			if (DistanceSquared(position, other.Position) < minimum_distance * minimum_distance)
			{
				return false;
			}
		}
		for (const EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			if (!slot.Entity.IsActive())
			{
				continue;
			}
			const DirectX::XMFLOAT2 other = slot.Entity.GetMapCollisionCenter();
			const float minimum_distance = collision_radius + slot.Entity.GetMapCollisionRadius() + SeparationPadding;
			if (DistanceSquared(position, other) < minimum_distance * minimum_distance)
			{
				return false;
			}
		}
		return true;
	}
} // namespace GameEnemy::Internal
