#include "game_enemy_internal.h"

#include "math_utils.h"
#include "game_enemy.h"
#include "Constants/enemy_constants.h"

namespace
{

	namespace EnemyTuning::Cthulhu
	{
		constexpr float EyeLaserCollisionHalfWidth = 34.0f;
		constexpr int EyeLaserBurstCount = 3;
	} // namespace EnemyTuning::Cthulhu

} // namespace

namespace GameEnemy::Internal
{
	// 보스 전용 탄막과 패턴은 일반 몬스터 공격과 따로 돌린다.
	void ClearBossJellyBullets()
	{
		for (BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
		{
			bullet = BossJellyBullet{};
		}
	}

	void ClearCorruptionBullets()
	{
		for (BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
		{
			if (bullet.Style != BossProjectileStyle::Jelly)
			{
				bullet.IsActive = false;
			}
		}
	}

	bool SpawnBossJellyBullet(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction, bool is_primary,
	                          float speed_scale = 1.0f, float size_scale = 1.0f)
	{
		static constexpr float SplitDamage = 8.0f;
		static constexpr float PrimaryDamage = 15.0f;
		static constexpr float SplitDistance = 680.0f;
		static constexpr float SplitSpeed = 285.0f;
		static constexpr float PrimarySpeed = 360.0f;
		for (BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
		{
			if (bullet.IsActive)
			{
				continue;
			}
			const float speed = (is_primary ? PrimarySpeed : SplitSpeed) * speed_scale;
			bullet = BossJellyBullet{};
			bullet.Position = position;
			bullet.Velocity = { direction.x * speed, direction.y * speed };
			bullet.Radius = (is_primary ? 30.0f : 18.0f) * size_scale;
			bullet.Size = (is_primary ? 84.0f : 52.0f) * size_scale;
			bullet.Damage = is_primary ? PrimaryDamage : SplitDamage;
			bullet.MaxDistance = is_primary ? EnemyConstants::BossJelly::PrimaryDistance : SplitDistance;
			bullet.Rotation = std::atan2(bullet.Velocity.x, -bullet.Velocity.y);
			bullet.IsPrimary = is_primary;
			bullet.Style = BossProjectileStyle::Jelly;
			bullet.IsActive = true;
			return true;
		}
		return false;
	}

	bool SpawnCorruptionBullet(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction, float speed,
	                           float size, float damage, float maximum_distance, BossProjectileStyle style,
	                           float turn_rate = 0.0f, float acceleration = 0.0f, float visual_scale = 1.0f)
	{
		for (BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
		{
			if (bullet.IsActive)
			{
				continue;
			}
			bullet = BossJellyBullet{};
			bullet.Position = position;
			bullet.Velocity = { direction.x * speed, direction.y * speed };
			// 보이는 탄보다 실제 판정을 작게 잡아 촘촘한 탄막도 피할 틈을 준다.
			bullet.Radius = size * 0.22f;
			bullet.Size = size;
			bullet.Damage = damage;
			bullet.MaxDistance = maximum_distance;
			bullet.Rotation = std::atan2(direction.y, direction.x);
			bullet.TurnRate = turn_rate;
			bullet.Acceleration = acceleration;
			bullet.VisualScale = visual_scale;
			bullet.Style = style;
			bullet.IsActive = true;
			return true;
		}
		return false;
	}

	void FireCorruptionFan(const EnemySlot& boss_slot, const DirectX::XMFLOAT2& player_position, int shot_count,
	                       float angle_step, float speed, float size, float damage, BossProjectileStyle style,
	                       float turn_rate = 0.0f)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const DirectX::XMFLOAT2 aim_direction = GetDirection(origin, player_position);
		const float center = static_cast<float>(shot_count - 1) * 0.5f;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const DirectX::XMFLOAT2 direction =
			    RotateDirection(aim_direction, (static_cast<float>(shot_index) - center) * angle_step);
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
			};
			SpawnCorruptionBullet(muzzle, direction, speed, size, damage, 1100.0f, style, turn_rate);
		}
	}

	void FireCorruptionRing(const EnemySlot& boss_slot, int shot_count, float angle_offset, float speed, float size,
	                        float damage, BossProjectileStyle style, float turn_rate = 0.0f,
	                        float gap_center_angle = 0.0f, float gap_half_angle = 0.0f, float visual_scale = 1.0f)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const float angle =
			    angle_offset + DirectX::XM_2PI * static_cast<float>(shot_index) / static_cast<float>(shot_count);
			if (gap_half_angle > 0.0f)
			{
				const float gap_delta =
				    std::atan2(std::sin(angle - gap_center_angle), std::cos(angle - gap_center_angle));
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
			SpawnCorruptionBullet(muzzle, direction, speed, size, damage, 1050.0f, style, turn_rate, 0.0f,
			                      visual_scale);
		}
	}

	float DistanceSquaredToSegment(const DirectX::XMFLOAT2& point, const DirectX::XMFLOAT2& start,
	                               const DirectX::XMFLOAT2& end)
	{
		const float segment_x = end.x - start.x;
		const float segment_y = end.y - start.y;
		const float length_squared = segment_x * segment_x + segment_y * segment_y;
		if (length_squared <= 0.0001f)
		{
			return DistanceSquared(point, start);
		}
		const float point_x = point.x - start.x;
		const float point_y = point.y - start.y;
		const float projection = Saturate((point_x * segment_x + point_y * segment_y) / length_squared);
		const DirectX::XMFLOAT2 nearest{
			start.x + segment_x * projection,
			start.y + segment_y * projection,
		};
		return DistanceSquared(point, nearest);
	}

	DirectX::XMFLOAT2 GetCthulhuEyeSpawnPosition(const DirectX::XMFLOAT2& player_position, int step)
	{
		static constexpr float EyeLaserSummonDistance = 420.0f;
		static constexpr float EdgePadding = 82.0f;
		const float sequence_rotation = static_cast<float>(g_EnemySystem.CorruptionVolleySequence % 5) * 0.19f;
		const float base_angle =
		    0.72f + sequence_rotation +
		    DirectX::XM_2PI * static_cast<float>(step) / static_cast<float>(EnemyTuning::Cthulhu::EyeLaserBurstCount);
		for (int attempt = 0; attempt < 12; ++attempt)
		{
			const float angle = base_angle + DirectX::XM_PI * static_cast<float>(attempt) / 6.0f;
			const DirectX::XMFLOAT2 candidate{
				player_position.x + std::cos(angle) * EyeLaserSummonDistance,
				player_position.y + std::sin(angle) * EyeLaserSummonDistance,
			};
			if (ProceduralMap_IsCircleWalkable(candidate, EnemyTuning::Cthulhu::EyeLaserCollisionHalfWidth + 12.0f))
			{
				return candidate;
			}
		}
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(ProceduralMap_GetRoomIndexAt(player_position));
		DirectX::XMFLOAT2 fallback{
			player_position.x + std::cos(base_angle) * EyeLaserSummonDistance,
			player_position.y + std::sin(base_angle) * EyeLaserSummonDistance,
		};
		if (room)
		{
			fallback.x = std::clamp(fallback.x, room->WorldMin.x + EdgePadding, room->WorldMax.x - EdgePadding);
			fallback.y = std::clamp(fallback.y, room->WorldMin.y + EdgePadding, room->WorldMax.y - EdgePadding);
		}
		return fallback;
	}

	void BeginCthulhuEyeLaser(const DirectX::XMFLOAT2& player_position, int step)
	{
		static constexpr float EyeLaserMaxDistance = 2400.0f;
		static constexpr float EyeLaserTelegraphDuration = 0.38f;
		CthulhuEyeLaser& laser = g_EnemySystem.CthulhuLaser;
		laser = {};
		laser.Start = GetCthulhuEyeSpawnPosition(player_position, step);
		laser.Direction = GetDirection(laser.Start, player_position);
		laser.End = ProceduralMap_TraceWalkableSegment(laser.Start, laser.Direction, EyeLaserMaxDistance,
		                                               EnemyTuning::Cthulhu::EyeLaserCollisionHalfWidth);
		laser.Timer = EyeLaserTelegraphDuration;
		laser.PhaseDuration = EyeLaserTelegraphDuration;
		laser.Phase = CthulhuEyeLaserPhase::Telegraph;
	}

	void UpdateCthulhuEyeLaser(float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float EyeLaserDamage = 16.0f;
		static constexpr float EyeLaserActiveDuration = 0.16f;
		static constexpr float PlayerCollisionRadius = 30.0f;
		CthulhuEyeLaser& laser = g_EnemySystem.CthulhuLaser;
		if (laser.Phase == CthulhuEyeLaserPhase::Inactive)
		{
			return;
		}
		laser.Timer -= delta_time;
		if (laser.Phase == CthulhuEyeLaserPhase::Telegraph && laser.Timer <= 0.0f)
		{
			laser.Phase = CthulhuEyeLaserPhase::Firing;
			laser.Timer = EyeLaserActiveDuration;
			laser.PhaseDuration = EyeLaserActiveDuration;
			laser.DamageApplied = false;
			if (g_EnemySystem.CthulhuLaserAudioID >= 0)
			{
				Audio_Play(g_EnemySystem.CthulhuLaserAudioID);
			}
		}
		if (laser.Phase != CthulhuEyeLaserPhase::Firing)
		{
			return;
		}
		const float hit_radius = EnemyTuning::Cthulhu::EyeLaserCollisionHalfWidth + PlayerCollisionRadius;
		if (!laser.DamageApplied &&
		    DistanceSquaredToSegment(player_position, laser.Start, laser.End) <= hit_radius * hit_radius)
		{
			GamePlayer::ApplyDamage(EyeLaserDamage);
			laser.DamageApplied = true;
		}
		if (laser.Timer <= 0.0f)
		{
			laser = {};
		}
	}

	void FireCthulhuAimedSalvo(const EnemySlot& boss_slot, const DirectX::XMFLOAT2& player_position, int step,
	                           bool phase_two, bool final_phase)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const DirectX::XMFLOAT2 aim = GetDirection(origin, player_position);
		const int shot_count = final_phase ? 11 : (phase_two ? 9 : 7);
		const float center = static_cast<float>(shot_count - 1) * 0.5f;
		const float sweep = static_cast<float>((step % 3) - 1) * 0.045f;
		const float speed = final_phase ? 465.0f : (phase_two ? 425.0f : 385.0f);
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const float angle = (static_cast<float>(shot_index) - center) * 0.125f + sweep;
			const DirectX::XMFLOAT2 direction = RotateDirection(aim, angle);
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 12.0f),
			};
			SpawnCorruptionBullet(muzzle, direction, speed, 30.0f, 9.0f, 1150.0f,
			                      (shot_index & 1) == 0 ? BossProjectileStyle::Abyss : BossProjectileStyle::MageViolet,
			                      0.0f, 0.0f, 1.16f);
		}
	}

	void FireCthulhuIris(const EnemySlot& boss_slot, const DirectX::XMFLOAT2& player_position, int step, bool phase_two,
	                     bool final_phase)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const DirectX::XMFLOAT2 aim = GetDirection(origin, player_position);
		const float aim_angle = std::atan2(aim.y, aim.x);
		const int shot_count = final_phase ? 28 : (phase_two ? 24 : 20);
		const float gap_half_angle = final_phase ? 0.26f : (phase_two ? 0.32f : 0.38f);
		const float turn_rate = (step & 1) == 0 ? 0.045f : -0.045f;
		FireCorruptionRing(boss_slot, shot_count, static_cast<float>(step) * 0.17f,
		                   final_phase ? 290.0f : (phase_two ? 265.0f : 235.0f), 30.0f, 8.0f,
		                   BossProjectileStyle::MageViolet, turn_rate, aim_angle, gap_half_angle, 1.16f);
	}

	void FireCthulhuGate(const DirectX::XMFLOAT2& player_position, int step, bool phase_two, bool final_phase)
	{
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(ProceduralMap_GetRoomIndexAt(player_position));
		if (!room)
		{
			return;
		}
		const bool horizontal_wall = (step & 1) == 0;
		const bool from_minimum_edge = ((step / 2) & 1) == 0;
		const int lane_count = final_phase ? 18 : (phase_two ? 15 : 12);
		const float lane_minimum = horizontal_wall ? room->WorldMin.y + 76.0f : room->WorldMin.x + 76.0f;
		const float lane_maximum = horizontal_wall ? room->WorldMax.y - 76.0f : room->WorldMax.x - 76.0f;
		const float safe_center =
		    std::clamp(horizontal_wall ? player_position.y : player_position.x, lane_minimum, lane_maximum);
		const float safe_half_width = final_phase ? 62.0f : (phase_two ? 72.0f : 88.0f);
		const float travel_distance = horizontal_wall ? room->WorldMax.x - room->WorldMin.x + 160.0f
		                                              : room->WorldMax.y - room->WorldMin.y + 160.0f;
		const float speed = final_phase ? 330.0f : (phase_two ? 295.0f : 260.0f);
		for (int lane = 0; lane < lane_count; ++lane)
		{
			const float t = lane_count > 1 ? static_cast<float>(lane) / static_cast<float>(lane_count - 1) : 0.5f;
			const float lane_position = std::lerp(lane_minimum, lane_maximum, t);
			if (std::abs(lane_position - safe_center) < safe_half_width)
			{
				continue;
			}
			DirectX::XMFLOAT2 position{};
			DirectX::XMFLOAT2 direction{};
			if (horizontal_wall)
			{
				position = { from_minimum_edge ? room->WorldMin.x + 70.0f : room->WorldMax.x - 70.0f, lane_position };
				direction = { from_minimum_edge ? 1.0f : -1.0f, 0.0f };
			}
			else
			{
				position = { lane_position, from_minimum_edge ? room->WorldMin.y + 70.0f : room->WorldMax.y - 70.0f };
				direction = { 0.0f, from_minimum_edge ? 1.0f : -1.0f };
			}
			SpawnCorruptionBullet(position, direction, speed, 32.0f, 9.0f, travel_distance,
			                      horizontal_wall ? BossProjectileStyle::MageAzure : BossProjectileStyle::Abyss, 0.0f,
			                      0.0f, 1.16f);
		}
	}

	void FireCthulhuSpiralBloom(const EnemySlot& boss_slot, int step, bool phase_two, bool final_phase)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const int spoke_count = final_phase ? 11 : (phase_two ? 9 : 7);
		const float angle_offset = static_cast<float>(step) * 0.34f;
		const float turn_rate = (step & 1) == 0 ? 0.28f : -0.28f;
		for (int spoke = 0; spoke < spoke_count; ++spoke)
		{
			const float angle =
			    angle_offset + DirectX::XM_2PI * static_cast<float>(spoke) / static_cast<float>(spoke_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 10.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 10.0f),
			};
			SpawnCorruptionBullet(muzzle, direction, final_phase ? 285.0f : (phase_two ? 260.0f : 235.0f), 28.0f, 8.0f,
			                      1100.0f, BossProjectileStyle::Abyss, turn_rate, 0.0f, 1.16f);
		}
		if (phase_two && (step & 1) != 0)
		{
			FireCorruptionRing(boss_slot, final_phase ? 11 : 8,
			                   -angle_offset + DirectX::XM_PI / static_cast<float>(spoke_count),
			                   final_phase ? 335.0f : 305.0f, 25.0f, 7.0f, BossProjectileStyle::MageAzure,
			                   -turn_rate * 0.55f, 0.0f, 0.0f, 1.16f);
		}
	}

	void FireCthulhuDashScatter(const EnemySlot& boss_slot, int sequence, bool phase_two, bool final_phase)
	{
		const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(boss_slot);
		const int bullet_count = final_phase ? 10 : (phase_two ? 8 : 6);
		const float speed = final_phase ? 320.0f : (phase_two ? 285.0f : 250.0f);
		const float angle_offset = static_cast<float>(sequence) * 0.29f;
		for (int bullet_index = 0; bullet_index < bullet_count; ++bullet_index)
		{
			const float angle =
			    angle_offset + DirectX::XM_2PI * static_cast<float>(bullet_index) / static_cast<float>(bullet_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 muzzle{
				origin.x + direction.x * (boss_slot.Entity.GetCollisionRadius() + 8.0f),
				origin.y + direction.y * (boss_slot.Entity.GetCollisionRadius() + 8.0f),
			};
			const float turn_rate = (bullet_index & 1) == 0 ? 0.10f : -0.10f;
			SpawnCorruptionBullet(muzzle, direction, speed + static_cast<float>(bullet_index % 3) * 18.0f, 27.0f, 7.0f,
			                      920.0f,
			                      (bullet_index & 1) == 0 ? BossProjectileStyle::Abyss : BossProjectileStyle::MageAzure,
			                      turn_rate, 0.0f, 1.16f);
		}
	}

	void UpdateCorruptedBossPattern(float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float DashBulletInterval = 0.11f;
		EnemySlot* boss_slot = FindAliveCorruptedBossSlot();
		if (!boss_slot)
		{
			g_EnemySystem.CorruptionFireCooldown = 0.8f;
			g_EnemySystem.CorruptionVolleySequence = 0;
			g_EnemySystem.CthulhuActivePattern = -1;
			g_EnemySystem.CthulhuPatternStep = 0;
			g_EnemySystem.CthulhuLaser = {};
			g_EnemySystem.CthulhuDashBulletCooldown = 0.0f;
			g_EnemySystem.CthulhuDashBulletSequence = 0;
			return;
		}
		const float health_ratio = boss_slot->Entity.GetHitPointRatio();
		const bool cthulhu = boss_slot->Type == MonsterType::BossCthulhu;
		const bool phase_two = health_ratio <= (cthulhu ? 0.62f : 0.55f);
		const bool final_phase = health_ratio <= (cthulhu ? 0.28f : 0.25f);
		if (cthulhu)
		{
			UpdateCthulhuEyeLaser(delta_time, player_position);
			if (EnemyAttackPattern::IsCthulhuDashing(GetEnemySlotID(boss_slot)))
			{
				g_EnemySystem.CthulhuDashBulletCooldown -= delta_time;
				const float interval = final_phase ? 0.075f : (phase_two ? 0.09f : DashBulletInterval);
				while (g_EnemySystem.CthulhuDashBulletCooldown <= 0.0f)
				{
					FireCthulhuDashScatter(*boss_slot, g_EnemySystem.CthulhuDashBulletSequence, phase_two, final_phase);
					++g_EnemySystem.CthulhuDashBulletSequence;
					g_EnemySystem.CthulhuDashBulletCooldown += interval;
				}
			}
			else
			{
				g_EnemySystem.CthulhuDashBulletCooldown = 0.0f;
				g_EnemySystem.CthulhuDashBulletSequence = 0;
			}
		}
		else
		{
			g_EnemySystem.CthulhuLaser = {};
			g_EnemySystem.CthulhuDashBulletCooldown = 0.0f;
			g_EnemySystem.CthulhuDashBulletSequence = 0;
		}
		if (boss_slot->Type == MonsterType::BossCthulhu &&
		    !EnemyAttackPattern::IsBossAttackWindow(GetEnemySlotID(boss_slot)))
		{
			// 각 시전은 애니메이션의 준비 동작이 끝난 뒤 발사 주기를 다시 시작한다.
			g_EnemySystem.CorruptionFireCooldown = 0.0f;
			g_EnemySystem.CthulhuActivePattern = -1;
			g_EnemySystem.CthulhuPatternStep = 0;
			return;
		}
		g_EnemySystem.CorruptionFireCooldown -= delta_time;
		if (g_EnemySystem.CorruptionFireCooldown > 0.0f)
		{
			return;
		}
		const int sequence = g_EnemySystem.CorruptionVolleySequence;
		switch (boss_slot->Type)
		{
		case MonsterType::BossCorruptedKnight:
		{
			// 검 표식: 조준 3연사 뒤 회전 벽을 만든다.
			const int cycle_length = phase_two ? 5 : 4;
			const int cycle_step = sequence % cycle_length;
			if (cycle_step < 3)
			{
				const float turn = (cycle_step & 1) == 0 ? 0.045f : -0.045f;
				FireCorruptionFan(*boss_slot, player_position, final_phase ? 7 : (phase_two ? 5 : 3), 0.125f,
				                  final_phase ? 520.0f : 480.0f, 36.0f, 10.0f, BossProjectileStyle::Knight, turn);
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.24f : 0.31f;
			}
			else
			{
				const DirectX::XMFLOAT2 origin = GetEnemyAimPosition(*boss_slot);
				const DirectX::XMFLOAT2 aim = GetDirection(origin, player_position);
				const float aim_angle = std::atan2(aim.y, aim.x);
				const float turn = (sequence & 1) == 0 ? 0.085f : -0.085f;
				FireCorruptionRing(*boss_slot, final_phase ? 18 : (phase_two ? 16 : 14), aim_angle + 0.11f,
				                   phase_two ? 305.0f : 275.0f, 34.0f, 8.0f, BossProjectileStyle::Knight, turn,
				                   aim_angle, 0.40f);
				if (phase_two && cycle_step == 4)
				{
					FireCorruptionRing(*boss_slot, final_phase ? 8 : 6, aim_angle + DirectX::XM_PIDIV4, 255.0f, 36.0f,
					                   8.0f, BossProjectileStyle::Knight, -turn);
				}
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.92f : 1.18f;
			}
			break;
		}
		case MonsterType::BossCorruptedMage:
		{
			// 별 표식: 서로 반대로 도는 두 줄의 나선 탄막.
			const int spoke_count = final_phase ? 5 : (phase_two ? 4 : 3);
			const float spiral_angle = static_cast<float>(sequence) * (final_phase ? 0.29f : 0.24f);
			FireCorruptionRing(*boss_slot, spoke_count, spiral_angle, final_phase ? 250.0f : 220.0f, 30.0f, 7.0f,
			                   BossProjectileStyle::MageViolet, final_phase ? 0.30f : 0.23f);
			FireCorruptionRing(*boss_slot, spoke_count,
			                   -spiral_angle + DirectX::XM_PI / static_cast<float>(spoke_count),
			                   final_phase ? 325.0f : 285.0f, 28.0f, 7.0f, BossProjectileStyle::MageAzure,
			                   final_phase ? -0.24f : -0.18f);
			if ((sequence % 5) == 4)
			{
				FireCorruptionFan(*boss_slot, player_position, phase_two ? 3 : 1, 0.18f, 390.0f, 32.0f, 9.0f,
				                  BossProjectileStyle::MageAzure);
			}
			g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.20f : (phase_two ? 0.24f : 0.30f);
			if ((sequence % 10) == 9)
			{
				g_EnemySystem.CorruptionFireCooldown += final_phase ? 0.90f : 1.25f;
			}
			break;
		}
		case MonsterType::BossCthulhu:
		{
			const int attack_variant = EnemyAttackPattern::GetBossAttackVariant(GetEnemySlotID(boss_slot));
			if (g_EnemySystem.CthulhuActivePattern != attack_variant)
			{
				g_EnemySystem.CthulhuActivePattern = attack_variant;
				g_EnemySystem.CthulhuPatternStep = 0;
			}
			const int pattern_step = g_EnemySystem.CthulhuPatternStep;
			switch (attack_variant)
			{
			case 0:
				// 눈 레이저: 각 발사 전에 회피 경로를 보여 주고 고정된 방향으로 세 번 발사한다.
				if (pattern_step < EnemyTuning::Cthulhu::EyeLaserBurstCount)
				{
					BeginCthulhuEyeLaser(player_position, pattern_step);
					g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.55f : (phase_two ? 0.57f : 0.60f);
				}
				else
				{
					g_EnemySystem.CorruptionFireCooldown = 10.0f;
				}
				break;
			case 1:
				// 응시: 조준 탄막을 짧게 반복해 발사 사이마다 이동 방향을 바꾸도록 유도한다.
				FireCthulhuAimedSalvo(*boss_slot, player_position, pattern_step, phase_two, final_phase);
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.21f : (phase_two ? 0.25f : 0.29f);
				break;
			case 2:
				// 홍채: 퍼지는 고리마다 플레이어 쪽에 부채꼴 탈출구를 남긴다.
				FireCthulhuIris(*boss_slot, player_position, pattern_step, phase_two, final_phase);
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.39f : (phase_two ? 0.46f : 0.54f);
				break;
			case 3:
				// 문: 가로·세로 탄막 벽을 번갈아 만들고 플레이어가 통과할 틈을 남긴다.
				FireCthulhuGate(player_position, pattern_step, phase_two, final_phase);
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.44f : (phase_two ? 0.52f : 0.61f);
				break;
			case 4:
			default:
				// 개화: 곡선형 방사 탄막을 내보낸 뒤 예고한 위치로 돌진한다.
				FireCthulhuSpiralBloom(*boss_slot, pattern_step, phase_two, final_phase);
				g_EnemySystem.CorruptionFireCooldown = final_phase ? 0.18f : (phase_two ? 0.22f : 0.27f);
				break;
			}
			++g_EnemySystem.CthulhuPatternStep;
			break;
		}
		default:
			g_EnemySystem.CorruptionFireCooldown = 1.0f;
			break;
		}
		++g_EnemySystem.CorruptionVolleySequence;
	}

	void SplitBossJellyBullet(const DirectX::XMFLOAT2& position)
	{
		// 방울 표식: 큰 탄이 두 겹 고리로 갈라지고 페이즈마다 탄 수가 늘어난다.
		const int outer_count = 8 + g_EnemySystem.BossSplitStage * 2;
		const int inner_count = 4 + g_EnemySystem.BossSplitStage;
		const float base_angle = static_cast<float>(g_EnemySystem.BossVolleySequence) * 0.23f;
		for (int direction_index = 0; direction_index < outer_count; ++direction_index)
		{
			const float angle =
			    base_angle + DirectX::XM_2PI * static_cast<float>(direction_index) / static_cast<float>(outer_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 spawn_position{
				position.x + direction.x * 12.0f,
				position.y + direction.y * 12.0f,
			};
			SpawnBossJellyBullet(spawn_position, direction, false, 1.08f, 0.86f);
		}
		for (int direction_index = 0; direction_index < inner_count; ++direction_index)
		{
			const float angle = base_angle + DirectX::XM_PI / static_cast<float>(inner_count) +
			                    DirectX::XM_2PI * static_cast<float>(direction_index) / static_cast<float>(inner_count);
			const DirectX::XMFLOAT2 direction{ std::cos(angle), std::sin(angle) };
			const DirectX::XMFLOAT2 spawn_position{
				position.x + direction.x * 8.0f,
				position.y + direction.y * 8.0f,
			};
			SpawnBossJellyBullet(spawn_position, direction, false, 0.64f, 0.66f);
		}
		cSlimeGoo::GetInstance().Spawn(position, { 0.0f, 0.0f }, 0.9f);
	}

	DirectX::XMFLOAT2 GetBossJellyMuzzlePosition(const EnemySlot& boss_slot, const DirectX::XMFLOAT2& direction)
	{
		const cEnemy& boss = boss_slot.Entity;
		const DirectX::XMFLOAT2 boss_position = GetEnemyAimPosition(boss_slot);
		const float muzzle_offset = boss.GetCollisionRadius() + 22.0f;
		return {
			boss_position.x + direction.x * muzzle_offset,
			boss_position.y + direction.y * muzzle_offset,
		};
	}

	DirectX::XMFLOAT2 GetBossJellyPrimaryDirection(const DirectX::XMFLOAT2& aim_direction, bool phase_two,
	                                               int shot_index)
	{
		static constexpr float PairAngleOffset = 0.18f;
		if (!phase_two)
		{
			return aim_direction;
		}
		const float angle_offset = shot_index == 0 ? -PairAngleOffset : PairAngleOffset;
		const float sine = std::sin(angle_offset);
		const float cosine = std::cos(angle_offset);
		return {
			aim_direction.x * cosine - aim_direction.y * sine,
			aim_direction.x * sine + aim_direction.y * cosine,
		};
	}

	void FireBossJellyBullet(EnemySlot& boss_slot, const DirectX::XMFLOAT2& aim_direction, bool phase_two)
	{
		boss_slot.BossFireScaleElapsed = 0.0f;
		const int shot_count = phase_two ? 2 : 1;
		bool fired = false;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const DirectX::XMFLOAT2 direction = GetBossJellyPrimaryDirection(aim_direction, phase_two, shot_index);
			const DirectX::XMFLOAT2 muzzle_position = GetBossJellyMuzzlePosition(boss_slot, direction);
			fired = SpawnBossJellyBullet(muzzle_position, direction, true) || fired;
		}
		if (fired && g_EnemySystem.BossSlimeProjectileFireAudioID >= 0)
		{
			Audio_Play(g_EnemySystem.BossSlimeProjectileFireAudioID);
		}
	}

	void BeginBossJellyTelegraph(const DirectX::XMFLOAT2& player_position)
	{
		EnemySlot* firing_boss = FindBossVolleySlot();
		if (!firing_boss)
		{
			g_EnemySystem.BossJellyTelegraphEnemyID = -1;
			return;
		}
		const DirectX::XMFLOAT2 boss_position = GetEnemyAimPosition(*firing_boss);
		const float dx = player_position.x - boss_position.x;
		const float dy = player_position.y - boss_position.y;
		const float distance_squared = LengthSquared({ dx, dy });
		if (distance_squared <= 0.0001f)
		{
			g_EnemySystem.BossJellyTelegraphEnemyID = -1;
			return;
		}
		g_EnemySystem.BossJellyTelegraphEnemyID = GetEnemySlotID(firing_boss);
		g_EnemySystem.BossJellyTelegraphDirection = NormalizeOr({ dx, dy }, { 1.0f, 0.0f });
	}

	void TrySplitBossBody(const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float SecondSplitSpeedScale = 1.70f;
		static constexpr float SecondSplitOffset = 86.0f;
		static constexpr float SecondSplitScale = 0.72f;
		static constexpr float SplitSpeedScale = 1.35f;
		static constexpr float SplitOffset = 118.0f;
		static constexpr float SplitScale = 0.68f;
		if (g_EnemySystem.BossSplitStage >= 2)
		{
			return;
		}
		const MonsterData& boss_data = GetMonsterData(MonsterType::BossSlime);
		std::array<int, 2> source_indices{ -1, -1 };
		std::array<int, 2> free_indices{ -1, -1 };
		int source_count = 0;
		float total_hit_point = 0.0f;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			const EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
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
		const float split_threshold =
		    g_EnemySystem.BossSplitStage == 0 ? boss_data.MaxHitPoint * 0.5f : boss_data.MaxHitPoint * 0.25f;
		if (total_hit_point > split_threshold)
		{
			return;
		}
		int free_count = 0;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY && free_count < source_count; ++enemy_id)
		{
			if (!g_EnemySystem.EnemySlots[enemy_id].Entity.IsActive())
			{
				free_indices[free_count++] = enemy_id;
			}
		}
		if (free_count < source_count)
		{
			return;
		}
		const float scale_multiplier = g_EnemySystem.BossSplitStage == 0 ? SplitScale : SecondSplitScale;
		const float split_offset = g_EnemySystem.BossSplitStage == 0 ? SplitOffset : SecondSplitOffset;
		const float speed_scale = g_EnemySystem.BossSplitStage == 0 ? SplitSpeedScale : SecondSplitSpeedScale;
		const float burst_intensity = g_EnemySystem.BossSplitStage == 0 ? 3.0f : 2.2f;
		for (int source_number = 0; source_number < source_count; ++source_number)
		{
			EnemySlot& source_slot = g_EnemySystem.EnemySlots[source_indices[source_number]];
			const DirectX::XMFLOAT2 source_position = source_slot.Entity.GetPosition();
			const float split_max_hit_point = source_slot.Entity.GetMaxHitPoint() * 0.5f;
			const float split_hit_point = source_slot.Entity.GetHitPoint() * 0.5f;
			const float split_radius = source_slot.Entity.GetCollisionRadius() * scale_multiplier;
			const int room_index = source_slot.RoomIndex;
			const float draw_scale = source_slot.DrawScale * scale_multiplier;
			const float experience_scale = source_slot.ExperienceScale * 0.5f;
			const MonsterMapCollisionShape map_collision =
			    GetMonsterMapCollisionShape(MonsterType::BossSlime, draw_scale);
			const DirectX::XMFLOAT2 to_player{
				player_position.x - source_position.x,
				player_position.y - source_position.y,
			};
			const DirectX::XMFLOAT2 to_player_direction = NormalizeOr(to_player, { 0.0f, -1.0f });
			DirectX::XMFLOAT2 tangent{ -to_player_direction.y, to_player_direction.x };
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
			    source_position, split_movement, split_radius + EnemyConstants::Collision::WallPadding);
			const DirectX::XMFLOAT2 second_position =
			    ProceduralMap_MoveActorCircle(source_position, { -split_movement.x, -split_movement.y },
			                                  split_radius + EnemyConstants::Collision::WallPadding);
			source_slot.Entity.Deactivate();
			auto spawn_half =
			    [&](EnemySlot& slot, const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& outward_direction)
			{
				cEnemy::SpawnSettings spawn_settings{};
				spawn_settings.Speed = boss_data.MoveSpeed * speed_scale;
				spawn_settings.MaxHitPoint = split_max_hit_point;
				spawn_settings.CollisionRadius = split_radius;
				spawn_settings.MapCollisionOffset = map_collision.Offset;
				spawn_settings.MapCollisionRadius = map_collision.Radius;
				slot.Entity.Spawn(position, spawn_settings);
				slot.Entity.SetHitPoint(split_hit_point);
				slot.Entity.ApplyKnockback(outward_direction, 180.0f);
				slot.Type = MonsterType::BossSlime;
				slot.RoomIndex = room_index;
				slot.DrawScale = draw_scale;
				slot.ExperienceScale = experience_scale;
				slot.BossFireScaleElapsed = EnemyConstants::BossJelly::FireScaleDuration;
				slot.BossSplitScaleElapsed = 0.0f;
				EnemyAttackPattern::OnSpawn(GetEnemySlotID(&slot), MonsterType::BossSlime);
			};
			spawn_half(source_slot, first_position, tangent);
			spawn_half(g_EnemySystem.EnemySlots[free_indices[source_number]], second_position,
			           { -tangent.x, -tangent.y });
			cSlimeGoo::GetInstance().Spawn(source_position, tangent, burst_intensity);
			cSlimeGoo::GetInstance().Spawn(source_position, { -tangent.x, -tangent.y }, burst_intensity);
		}
		++g_EnemySystem.BossSplitStage;
		g_EnemySystem.BossVolleySequence = 0;
		g_EnemySystem.BossJellyFireCooldown = 0.75f;
		g_EnemySystem.BossJellyTelegraphEnemyID = -1;
	}

	void UpdateBossJellyPattern(float delta_time, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float PhaseTwoFireInterval = 2.05f;
		EnemySlot* boss_slot = FindAliveBossSlot();
		if (!boss_slot)
		{
			g_EnemySystem.BossJellyFireCooldown = EnemyConstants::BossJelly::FireInterval;
			g_EnemySystem.BossJellyTelegraphEnemyID = -1;
		}
		else
		{
			const bool phase_two = g_EnemySystem.BossSplitStage > 0 || boss_slot->Entity.GetHitPointRatio() <= 0.5f;
			g_EnemySystem.BossJellyFireCooldown -= delta_time;
			if (g_EnemySystem.BossJellyTelegraphEnemyID < 0 &&
			    g_EnemySystem.BossJellyFireCooldown <= EnemyConstants::BossJelly::TelegraphDuration)
			{
				BeginBossJellyTelegraph(player_position);
			}
			if (g_EnemySystem.BossJellyFireCooldown <= 0.0f)
			{
				if (IsValidEnemyID(g_EnemySystem.BossJellyTelegraphEnemyID))
				{
					EnemySlot& firing_boss = g_EnemySystem.EnemySlots[g_EnemySystem.BossJellyTelegraphEnemyID];
					if (firing_boss.Type == MonsterType::BossSlime && firing_boss.Entity.IsAlive())
					{
						FireBossJellyBullet(firing_boss, g_EnemySystem.BossJellyTelegraphDirection, phase_two);
					}
				}
				g_EnemySystem.BossJellyTelegraphEnemyID = -1;
				g_EnemySystem.BossJellyFireCooldown +=
				    phase_two ? PhaseTwoFireInterval : EnemyConstants::BossJelly::FireInterval;
			}
		}
		for (BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
		{
			if (!bullet.IsActive)
			{
				continue;
			}
			if (std::abs(bullet.TurnRate) > 0.0001f)
			{
				bullet.Velocity = RotateDirection(bullet.Velocity, bullet.TurnRate * delta_time);
			}
			if (std::abs(bullet.Acceleration) > 0.0001f)
			{
				const float speed = Length(bullet.Velocity);
				if (speed > 0.0001f)
				{
					const float next_speed = std::max(40.0f, speed + bullet.Acceleration * delta_time);
					const float speed_scale = next_speed / speed;
					bullet.Velocity.x *= speed_scale;
					bullet.Velocity.y *= speed_scale;
				}
			}
			bullet.Rotation = std::atan2(bullet.Velocity.y, bullet.Velocity.x);
			const DirectX::XMFLOAT2 previous_position = bullet.Position;
			const DirectX::XMFLOAT2 next_position{
				previous_position.x + bullet.Velocity.x * delta_time,
				previous_position.y + bullet.Velocity.y * delta_time,
			};
			const float step_x = next_position.x - previous_position.x;
			const float step_y = next_position.y - previous_position.y;
			const float step_distance = Length({ step_x, step_y });
			const bool hit_wall = !ProceduralMap_IsSegmentWalkable(previous_position, next_position, bullet.Radius);
			const bool reached_range = bullet.Travelled + step_distance >= bullet.MaxDistance;
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
} // namespace GameEnemy::Internal
