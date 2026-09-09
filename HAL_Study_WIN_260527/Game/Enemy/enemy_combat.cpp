#include "game_enemy_internal.h"

#include "math_utils.h"
#include "game_enemy.h"
#include "Constants/player_constants.h"
#include "Constants/enemy_constants.h"

namespace GameEnemy
{
	using namespace Internal;

	// 자동 조준, 연쇄 공격, 보스 체력처럼 외부에서 쓰는 전투 함수들.
	bool IsAliveAndTargetable(int enemy_id)
	{
		return IsValidEnemyID(enemy_id) && g_EnemySystem.EnemySlots[enemy_id].Entity.IsAlive() &&
		       EnemyAttackPattern::IsTargetable(enemy_id);
	}

	bool FindNearestAlive(const DirectX::XMFLOAT2& origin, DirectX::XMFLOAT2& out_position)
	{
		if (!g_EnemySystem.CollisionGrid.HasAliveEnemy)
		{
			return false;
		}

		const int origin_cell_x = WorldToGridCell(origin.x, g_EnemySystem.EnemyCollisionCellSize);
		const int origin_cell_y = WorldToGridCell(origin.y, g_EnemySystem.EnemyCollisionCellSize);
		const int max_ring = std::max({
		    std::abs(origin_cell_x - g_EnemySystem.CollisionGrid.MinCellX),
		    std::abs(origin_cell_x - g_EnemySystem.CollisionGrid.MaxCellX),
		    std::abs(origin_cell_y - g_EnemySystem.CollisionGrid.MinCellY),
		    std::abs(origin_cell_y - g_EnemySystem.CollisionGrid.MaxCellY),
		});

		bool found = false;
		float best_distance_sq = 0.0f;
		DirectX::XMFLOAT2 best_position{};
		for (int ring = 0; ring <= max_ring; ++ring)
		{
			if (ring == 0)
			{
				CheckNearestEnemyInCell(origin_cell_x, origin_cell_y, origin, found, best_distance_sq, best_position);
			}
			else
			{
				const int min_cell_x = origin_cell_x - ring;
				const int max_cell_x = origin_cell_x + ring;
				const int min_cell_y = origin_cell_y - ring;
				const int max_cell_y = origin_cell_y + ring;
				for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
				{
					CheckNearestEnemyInCell(cell_x, min_cell_y, origin, found, best_distance_sq, best_position);
					CheckNearestEnemyInCell(cell_x, max_cell_y, origin, found, best_distance_sq, best_position);
				}
				for (int cell_y = min_cell_y + 1; cell_y < max_cell_y; ++cell_y)
				{
					CheckNearestEnemyInCell(min_cell_x, cell_y, origin, found, best_distance_sq, best_position);
					CheckNearestEnemyInCell(max_cell_x, cell_y, origin, found, best_distance_sq, best_position);
				}
			}

			if (found)
			{
				const float searched_left =
				    static_cast<float>(origin_cell_x - ring) * g_EnemySystem.EnemyCollisionCellSize;
				const float searched_right =
				    static_cast<float>(origin_cell_x + ring + 1) * g_EnemySystem.EnemyCollisionCellSize;
				const float searched_top =
				    static_cast<float>(origin_cell_y - ring) * g_EnemySystem.EnemyCollisionCellSize;
				const float searched_bottom =
				    static_cast<float>(origin_cell_y + ring + 1) * g_EnemySystem.EnemyCollisionCellSize;
				const float nearest_unsearched_distance = std::min({
				    origin.x - searched_left,
				    searched_right - origin.x,
				    origin.y - searched_top,
				    searched_bottom - origin.y,
				});
				if (best_distance_sq <= nearest_unsearched_distance * nearest_unsearched_distance)
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

	bool FindNearestAliveForChain(const DirectX::XMFLOAT2& origin, const int* excluded_enemy_ids, int excluded_count,
	                              float max_distance, int& out_enemy_id, DirectX::XMFLOAT2& out_position)
	{
		if (max_distance <= 0.0f)
		{
			return false;
		}

		bool found = false;
		float best_distance_sq = max_distance * max_distance;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			if (!IsAliveAndTargetable(enemy_id))
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

			const DirectX::XMFLOAT2 position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			const float distance_sq = DistanceSquared(position, origin);
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
		if (damage <= 0.0f || !IsAliveAndTargetable(enemy_id))
		{
			return false;
		}

		EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
		cEnemy& enemy = slot.Entity;
		const DirectX::XMFLOAT2 hit_position = GetEnemyAimPosition(slot);
		GameDamageText::Spawn(damage, hit_position);
		enemy.ApplyDamage(damage);
		const bool was_killed = !enemy.IsAlive();
		RecordCombatFeedback(slot, hit_position, { 0.0f, 0.0f }, damage, was_killed, false);
		PlayDamageReaction(slot.Type, hit_position, { 0.0f, 0.0f }, was_killed);
		if (was_killed)
		{
			HandleEnemyDefeat(slot, hit_position);
		}
		return true;
	}

	bool ConsumeCombatFeedback(CombatFeedback& out_feedback)
	{
		if (g_EnemySystem.CombatFeedback.HitCount <= 0)
		{
			return false;
		}

		out_feedback = g_EnemySystem.CombatFeedback;
		g_EnemySystem.CombatFeedback = {};
		return true;
	}

	int ApplyDashSlashDamage(const DirectX::XMFLOAT2& start, const DirectX::XMFLOAT2& end, float half_width,
	                         float damage)
	{
		if (half_width <= 0.0f || damage <= 0.0f)
		{
			return 0;
		}

		const float segment_x = end.x - start.x;
		const float segment_y = end.y - start.y;
		const float segment_length_sq = DistanceSquared(start, end);
		if (segment_length_sq <= 1.0f)
		{
			return 0;
		}

		const DirectX::XMFLOAT2 slash_direction = NormalizeOr({ segment_x, segment_y }, { 1.0f, 0.0f });
		PendingDashSlashAttack pending_attack{};
		pending_attack.Direction = slash_direction;
		pending_attack.Damage = damage;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			const cEnemy& enemy = g_EnemySystem.EnemySlots[enemy_id].Entity;
			if (!IsAliveAndTargetable(enemy_id))
			{
				continue;
			}

			const DirectX::XMFLOAT2 enemy_position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			const float relative_x = enemy_position.x - start.x;
			const float relative_y = enemy_position.y - start.y;
			const float amount = Saturate((relative_x * segment_x + relative_y * segment_y) / segment_length_sq);
			const float nearest_x = start.x + segment_x * amount;
			const float nearest_y = start.y + segment_y * amount;
			const float hit_radius = half_width + enemy.GetCollisionRadius();
			if (DistanceSquared(enemy_position, { nearest_x, nearest_y }) > hit_radius * hit_radius)
			{
				continue;
			}

			pending_attack.Targets.push_back({ enemy_id, enemy_position, false });
		}

		const int target_count = static_cast<int>(pending_attack.Targets.size());
		if (target_count > 0)
		{
			g_EnemySystem.PendingDashSlashAttacks.push_back(std::move(pending_attack));
		}
		return target_count;
	}

	bool UpdateDashSlashAttacks(float delta_time)
	{
		const float safe_delta_time = std::max(delta_time, 0.0f);
		bool applied_hit = false;
		for (PendingDashSlashAttack& attack : g_EnemySystem.PendingDashSlashAttacks)
		{
			attack.Elapsed += safe_delta_time;
			while (attack.NextHitIndex < PlayerConstants::Slash::CutCount &&
			       attack.Elapsed >= attack.NextHitIndex * PlayerConstants::Slash::CutInterval)
			{
				ApplyDashSlashPulse(attack);
				applied_hit = true;
				++attack.NextHitIndex;
			}
		}
		std::erase_if(g_EnemySystem.PendingDashSlashAttacks,
		              [](const PendingDashSlashAttack& attack)
		              {
			              return attack.NextHitIndex >= PlayerConstants::Slash::CutCount;
		              });
		return applied_hit;
	}

	bool TryGetAlivePosition(int enemy_id, DirectX::XMFLOAT2& out_position)
	{
		if (!IsAliveAndTargetable(enemy_id))
		{
			return false;
		}

		out_position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
		return true;
	}

	bool IsRoundCleared()
	{
		return g_EnemySystem.RoundCleared;
	}

	bool HasPendingBossSpawn()
	{
		return std::any_of(g_EnemySystem.PendingEnemySpawns.begin(), g_EnemySystem.PendingEnemySpawns.end(),
		                   [](const PendingEnemySpawn& pending)
		                   {
			                   return IsBossType(pending.Type);
		                   });
	}

	bool ConsumeBossDefeatedEvent(BossDefeatedEvent& out_event)
	{
		if (!g_EnemySystem.BossDefeatedEventPending)
		{
			return false;
		}

		out_event = g_EnemySystem.PendingBossDefeat;
		g_EnemySystem.BossDefeatedEventPending = false;
		return true;
	}

	void CompleteBossDefeat()
	{
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
			if (!IsBossType(slot.Type) || !slot.Entity.IsActive())
			{
				continue;
			}

			slot.Entity.Deactivate();
			slot.RoomIndex = -1;
			EnemyAttackPattern::OnDeactivate(enemy_id);
		}

		ClearBossJellyBullets();
		EnemyAttackPattern::Reset();
		BuildEnemyCollisionGrid();
	}

	bool TryGetBossHealth(float& out_hit_point, float& out_max_hit_point)
	{
		out_hit_point = 0.0f;
		out_max_hit_point = 0.0f;
		bool found = false;
		for (const EnemySlot& slot : g_EnemySystem.EnemySlots)
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
		case MonsterType::BossSlime:
			return "GIANT SLIME";
		case MonsterType::BossCorruptedKnight:
			return "CORRUPTED KNIGHT";
		case MonsterType::BossCorruptedMage:
			return "CORRUPTED MAGE";
		case MonsterType::BossCthulhu:
			return "ABYSS CTHULHU";
		default:
			return "CORRUPTED BOSS";
		}
	}

	bool IsRoomDiscovered(int room_index)
	{
		return room_index >= 0 && room_index < static_cast<int>(g_EnemySystem.DiscoveredRooms.size()) &&
		       g_EnemySystem.DiscoveredRooms[room_index];
	}

	bool IsRoomCleared(int room_index)
	{
		return room_index >= 0 && room_index < static_cast<int>(g_EnemySystem.RoomWaves.size()) &&
		       g_EnemySystem.RoomWaves[room_index].State == RoomWaveState::Cleared;
	}
} // namespace GameEnemy

namespace GameEnemy::Internal
{
	// 충돌 결과를 실제 적 피해로 바꾸는 내부 함수들.
	bool TryGetEnemyAndProjectile(const cCollisionHit& hit, int& enemy_id, int& projectile_id)
	{
		if (hit.BodyA.Layer == CollisionLayer::Enemy && hit.BodyB.Layer == CollisionLayer::PlayerBullet)
		{
			enemy_id = hit.BodyA.OwnerID;
			projectile_id = hit.BodyB.OwnerID;
			return true;
		}
		if (hit.BodyA.Layer == CollisionLayer::PlayerBullet && hit.BodyB.Layer == CollisionLayer::Enemy)
		{
			enemy_id = hit.BodyB.OwnerID;
			projectile_id = hit.BodyA.OwnerID;
			return true;
		}
		return false;
	}

	void ApplyProjectileDamage(int enemy_id, float damage, const DirectX::XMFLOAT2& knockback_direction,
	                           float knockback_speed)
	{
		if (!IsValidEnemyID(enemy_id) || damage <= 0.0f || !g_EnemySystem.EnemySlots[enemy_id].Entity.IsAlive())
		{
			return;
		}

		EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
		cEnemy& enemy = slot.Entity;
		const DirectX::XMFLOAT2 hit_position = GetEnemyAimPosition(slot);
		GameDamageText::Spawn(damage, hit_position);
		ApplyCombatKnockback(slot, knockback_direction, knockback_speed);
		enemy.ApplyDamage(damage);
		const bool was_killed = !enemy.IsAlive();
		RecordCombatFeedback(slot, hit_position, knockback_direction, damage, was_killed, false);
		const float impact_angle = std::atan2(knockback_direction.y, knockback_direction.x);
		const float impact_scale = std::clamp(0.40f + damage * 0.025f + (was_killed ? 0.22f : 0.0f), 0.42f, 0.92f);
		cGameEffectManager::GetInstance().Play(GameEffectType::DashSlashHitBurst, hit_position, impact_scale,
		                                       was_killed ? DirectX::XMFLOAT4{ 1.0f, 0.92f, 0.48f, 1.0f }
		                                                  : DirectX::XMFLOAT4{ 0.72f, 0.94f, 1.0f, 0.92f },
		                                       impact_angle);
		PlayDamageReaction(slot.Type, hit_position, knockback_direction, was_killed);
		if (was_killed)
		{
			HandleEnemyDefeat(slot, hit_position);
		}
	}

	void ApplyAreaProjectileDamage(const DirectX::XMFLOAT2& center, float radius, float damage,
	                               const DirectX::XMFLOAT2& fallback_knockback_direction)
	{
		if (radius <= 0.0f || damage <= 0.0f)
		{
			return;
		}

		GameBullet::PlayFireballExplosionSound();
		cGameEffectManager::GetInstance().PlayAreaExplosion(center, radius);

		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			const cEnemy& enemy = g_EnemySystem.EnemySlots[enemy_id].Entity;
			if (!enemy.IsAlive())
			{
				continue;
			}
			const float hit_radius = radius + enemy.GetCollisionRadius();
			const float hit_radius_sq = hit_radius * hit_radius;

			const DirectX::XMFLOAT2 enemy_position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			const DirectX::XMFLOAT2 from_center = {
				enemy_position.x - center.x,
				enemy_position.y - center.y,
			};
			const float distance_sq = LengthSquared(from_center);
			if (distance_sq > hit_radius_sq)
			{
				continue;
			}

			const DirectX::XMFLOAT2 knockback_direction =
			    distance_sq > 0.0001f ? from_center : fallback_knockback_direction;
			ApplyProjectileDamage(enemy_id, damage, knockback_direction,
			                      EnemyConstants::Knockback::PlayerBulletKnockbackSpeed * 1.35f);
		}
	}
} // namespace GameEnemy::Internal
