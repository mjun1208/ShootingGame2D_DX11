#include "enemy_attack_pattern_internal.h"
#include "Constants/enemy_pattern_constants.h"

namespace
{
	struct BatDashSettings
	{
		float TriggerRange;
		float MinimumRange;
		float MaxDistance;
		float Overshoot;
		float Speed;
	};

	constexpr BatDashSettings BatDash{
		.TriggerRange = 520.0f,
		.MinimumRange = 120.0f,
		.MaxDistance = 620.0f,
		.Overshoot = 140.0f,
		.Speed = 1500.0f,
	};

	struct SlimeDashSettings
	{
		float TriggerRange;
		float MinimumRange;
		float Distance;
		float Speed;
	};

	constexpr SlimeDashSettings SlimeDash{
		.TriggerRange = 540.0f,
		.MinimumRange = 90.0f,
		.Distance = 300.0f,
		.Speed = 900.0f,
	};

} // namespace

namespace EnemyAttackPattern::Internal
{
	// 박쥐: 준비 동작 후 플레이어 방향으로 돌진한다.
	void BeginBatWindup(EnemyPatternRuntime& runtime, const cEnemy& enemy, const DirectX::XMFLOAT2& player_position,
	                    float animation_elapsed)
	{
		const float distance = Distance(enemy.GetPosition(), player_position);
		runtime.State = ActionState::BatWindup;
		runtime.Timer = EnemyPatternConstants::Bat::DashWindupDuration;
		runtime.AnimationElapsed = animation_elapsed;
		runtime.DashDistance = std::min(BatDash.MaxDistance, distance + BatDash.Overshoot);
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.LockedDirection = GetDirection(runtime.TelegraphStart, player_position);
		runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
		                                                          runtime.DashDistance, enemy.GetCollisionRadius());
		runtime.TelegraphWidth = enemy.GetCollisionRadius() * 1.65f;
	}

	void UpdateBat(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	               const DirectX::XMFLOAT2& player_position, float animation_elapsed)
	{
		switch (runtime.State)
		{
		case ActionState::BatCooldown:
		{
			enemy.Update(delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= BatDash.TriggerRange * BatDash.TriggerRange &&
			    distance_squared >= BatDash.MinimumRange * BatDash.MinimumRange)
			{
				BeginBatWindup(runtime, enemy, player_position, animation_elapsed);
			}
			break;
		}
		case ActionState::BatWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed = std::fmod(runtime.AnimationElapsed + delta_time * 2.0f, 60.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
			                                                          runtime.DashDistance, enemy.GetCollisionRadius());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const float path_distance = Distance(runtime.TelegraphStart, runtime.TelegraphEnd);
				runtime.State = ActionState::BatDash;
				runtime.Timer = path_distance / BatDash.Speed;
				if (g_BatDashAudioID >= 0)
				{
					Audio_Play(g_BatDashAudioID);
				}
			}
			break;
		case ActionState::BatDash:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
			    runtime.LockedDirection.x * BatDash.Speed * movement_time,
			    runtime.LockedDirection.y * BatDash.Speed * movement_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::BatCooldown;
				runtime.Timer = GetRandomDuration(2.4f, 3.6f);
			}
			break;
		}
		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}
} // namespace EnemyAttackPattern::Internal

namespace EnemyAttackPattern::Internal
{
	// 스켈레톤 계열의 원거리 공격 패턴.
	void UpdateSkeleton(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                    const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float ThrowWindupDuration = 0.34f;
		static constexpr float ThrowMinimumRange = 220.0f;
		static constexpr float ThrowRange = 760.0f;
		switch (runtime.State)
		{
		case ActionState::SkeletonCooldown:
		{
			UpdateRangedMovement(enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= ThrowRange * ThrowRange &&
			    distance_squared >= ThrowMinimumRange * ThrowMinimumRange)
			{
				runtime.LockedDirection = GetDirection(GetAttackOrigin(enemy), player_position);
				runtime.State = ActionState::SkeletonWindup;
				runtime.Timer = ThrowWindupDuration;
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
				SpawnBoneProjectile(
				    {
				        position.x + runtime.LockedDirection.x * muzzle_offset,
				        position.y + runtime.LockedDirection.y * muzzle_offset,
				    },
				    runtime.LockedDirection, enemy_id);
				runtime.State = ActionState::SkeletonCooldown;
				runtime.Timer = GetRandomDuration(3.4f, 4.8f);
			}
			break;
		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}

	void UpdateSkeletonMage(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                        const DirectX::XMFLOAT2& player_position, float visual_bottom_offset)
	{
		static constexpr float CastMinimumRange = 240.0f;
		static constexpr float CastRange = 780.0f;
		switch (runtime.State)
		{
		case ActionState::SkeletonMageCooldown:
		{
			UpdateRangedMovement(enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= CastRange * CastRange &&
			    distance_squared >= CastMinimumRange * CastMinimumRange)
			{
				runtime.TelegraphStart = GetAttackOrigin(enemy);
				runtime.LockedDirection = GetDirection(runtime.TelegraphStart, player_position);
				runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(
				    runtime.TelegraphStart, runtime.LockedDirection, EnemyPatternConstants::MageProjectile::MaxDistance,
				    EnemyPatternConstants::MageProjectile::Radius);
				runtime.TelegraphWidth = 8.0f;
				const DirectX::XMFLOAT2 position = enemy.GetPosition();
				runtime.TargetPosition = {
					position.x,
					position.y + visual_bottom_offset,
				};
				runtime.State = ActionState::SkeletonMageWindup;
				runtime.Timer = EnemyPatternConstants::SkeletonMage::CastDuration;
			}
			break;
		}
		case ActionState::SkeletonMageWindup:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(
			    runtime.TelegraphStart, runtime.LockedDirection, EnemyPatternConstants::MageProjectile::MaxDistance,
			    EnemyPatternConstants::MageProjectile::Radius);
			const DirectX::XMFLOAT2 position = enemy.GetPosition();
			runtime.TargetPosition = {
				position.x,
				position.y + visual_bottom_offset,
			};
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				SpawnMageProjectile(GetAttackOrigin(enemy), runtime.LockedDirection, enemy_id);
				runtime.State = ActionState::SkeletonMageCooldown;
				runtime.Timer = GetRandomDuration(3.2f, 4.5f);
			}
			break;
		}
		default:
			runtime.State = ActionState::SkeletonMageCooldown;
			runtime.Timer = 1.0f;
			UpdateRangedMovement(enemy, delta_time, player_position);
			break;
		}
	}

	void UpdateSkeletonRogue(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                         const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float ThrowMinimumRange = 180.0f;
		static constexpr float ThrowRange = 620.0f;
		switch (runtime.State)
		{
		case ActionState::SkeletonRogueCooldown:
		{
			UpdateRangedMovement(enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= ThrowRange * ThrowRange &&
			    distance_squared >= ThrowMinimumRange * ThrowMinimumRange)
			{
				runtime.TelegraphStart = GetAttackOrigin(enemy);
				runtime.LockedDirection = GetDirection(runtime.TelegraphStart, player_position);
				runtime.State = ActionState::SkeletonRogueWindup;
				runtime.Timer = EnemyPatternConstants::SkeletonRogue::WindupDuration;
			}
			break;
		}
		case ActionState::SkeletonRogueWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				FireDaggerFan(runtime.TelegraphStart, runtime.LockedDirection, enemy.GetCollisionRadius() + 12.0f);
				runtime.State = ActionState::SkeletonRogueCooldown;
				runtime.Timer = GetRandomDuration(2.6f, 3.8f);
			}
			break;
		default:
			runtime.State = ActionState::SkeletonRogueCooldown;
			runtime.Timer = 1.0f;
			UpdateRangedMovement(enemy, delta_time, player_position);
			break;
		}
	}
} // namespace EnemyAttackPattern::Internal

namespace EnemyAttackPattern::Internal
{
	// 슬라임: 추적/대기 -> 돌진 예고 -> 돌진.
	void BeginSlimeTelegraph(EnemyPatternRuntime& runtime, const cEnemy& enemy,
	                         const DirectX::XMFLOAT2& player_position)
	{
		runtime.State = ActionState::SlimeTelegraph;
		runtime.Timer = EnemyPatternConstants::Slime::DashTelegraphDuration;
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.LockedDirection = GetDirection(runtime.TelegraphStart, player_position);
		runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
		                                                          SlimeDash.Distance, enemy.GetCollisionRadius());
		runtime.TelegraphWidth = enemy.GetCollisionRadius() * 2.25f;
	}

	void UpdateSlime(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position)
	{
		switch (runtime.State)
		{
		case ActionState::SlimeCooldown:
		{
			enemy.Update(delta_time, player_position);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(enemy.GetPosition(), player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= SlimeDash.TriggerRange * SlimeDash.TriggerRange &&
			    distance_squared >= SlimeDash.MinimumRange * SlimeDash.MinimumRange)
			{
				BeginSlimeTelegraph(runtime, enemy, player_position);
			}
			break;
		}
		case ActionState::SlimeTelegraph:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
			                                                          SlimeDash.Distance, enemy.GetCollisionRadius());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SlimeDash;
				runtime.Timer = SlimeDash.Distance / SlimeDash.Speed;
			}
			break;
		case ActionState::SlimeDash:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
			    runtime.LockedDirection.x * SlimeDash.Speed * movement_time,
			    runtime.LockedDirection.y * SlimeDash.Speed * movement_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::SlimeCooldown;
				runtime.Timer = GetRandomDuration(3.2f, 4.8f);
			}
			break;
		}
		default:
			enemy.Update(delta_time, player_position);
			break;
		}
	}
} // namespace EnemyAttackPattern::Internal

namespace EnemyAttackPattern::Internal
{
	// 주술사: 플레이어 위치에 장판을 예고하고 생성한다.
	void UpdateShaman(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                  const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float CastRange = 720.0f;
		switch (runtime.State)
		{
		case ActionState::ShamanCooldown:
			UpdateRangedMovement(enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f && DistanceSquared(enemy.GetPosition(), player_position) <= CastRange * CastRange)
			{
				runtime.State = ActionState::ShamanCast;
				runtime.Timer = EnemyPatternConstants::Shaman::CastDuration;
				runtime.TargetPosition = player_position;
			}
			break;
		case ActionState::ShamanCast:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				SpawnGroundHazard(runtime.TargetPosition);
				runtime.State = ActionState::ShamanCooldown;
				runtime.Timer = GetRandomDuration(3.8f, 5.2f);
			}
			break;
		default:
			runtime.State = ActionState::ShamanCooldown;
			runtime.Timer = 1.0f;
			UpdateRangedMovement(enemy, delta_time, player_position);
			break;
		}
	}
} // namespace EnemyAttackPattern::Internal

namespace EnemyAttackPattern::Internal
{
	// 전사, 오크, 도적 계열의 근거리 패턴.
	void UpdateWarrior(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                   const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float StrikeOffset = 62.0f;
		static constexpr float AttackRecoveryDuration = 0.42f;
		static constexpr float AttackTriggerDistance = 132.0f;
		static constexpr int SidestepCount = 2;
		static constexpr float SidestepSpeed = 760.0f;
		static constexpr float SidestepTriggerDistance = 480.0f;
		static constexpr float ChaseSpeedScale = 1.42f;
		switch (runtime.State)
		{
		case ActionState::WarriorChase:
		{
			const bool orc_warrior = runtime.Type == MonsterType::OrcWarrior;
			enemy.Update(delta_time, player_position, orc_warrior ? ChaseSpeedScale : 1.18f);
			runtime.Timer -= delta_time;
			const float distance_squared = DistanceSquared(GetAttackOrigin(enemy), player_position);
			const float action_trigger_distance = orc_warrior ? SidestepTriggerDistance : AttackTriggerDistance;
			if (runtime.Timer <= 0.0f && distance_squared <= action_trigger_distance * action_trigger_distance)
			{
				if (orc_warrior)
				{
					BeginOrcWarriorSidestep(runtime, enemy);
				}
				else
				{
					runtime.LockedDirection = GetDirection(GetAttackOrigin(enemy), player_position);
					runtime.State = ActionState::WarriorWindup;
					runtime.Timer = EnemyPatternConstants::Warrior::AttackWindupDuration;
				}
			}
			break;
		}
		case ActionState::WarriorSidestep:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const DirectX::XMFLOAT2 sidestep_direction =
			    GetOrcWarriorSidestepDirection(GetAttackOrigin(enemy), player_position, runtime.StrafeDirection);
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
			    sidestep_direction.x * SidestepSpeed * movement_time,
			    sidestep_direction.y * SidestepSpeed * movement_time,
			});
			WarriorDashTrailRuntime& active_trail =
			    runtime.WarriorDashTrails[static_cast<std::size_t>(runtime.MovementStep) %
			                              EnemyPatternConstants::OrcWarrior::DashTrailCount];
			active_trail.End = enemy.GetPosition();
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				++runtime.MovementStep;
				if (runtime.MovementStep < SidestepCount)
				{
					runtime.StrafeDirection *= -1.0f;
					runtime.Timer = EnemyPatternConstants::OrcWarrior::SidestepDuration;
					BeginOrcWarriorDashTrail(runtime, enemy.GetPosition());
				}
				else
				{
					const float distance_squared = DistanceSquared(GetAttackOrigin(enemy), player_position);
					if (distance_squared <= AttackTriggerDistance * AttackTriggerDistance)
					{
						runtime.LockedDirection = GetDirection(GetAttackOrigin(enemy), player_position);
						runtime.State = ActionState::WarriorWindup;
						runtime.Timer = EnemyPatternConstants::Warrior::AttackWindupDuration;
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
				runtime.TelegraphStart.x +
				    runtime.LockedDirection.x * (StrikeOffset + EnemyPatternConstants::Warrior::StrikeRadius),
				runtime.TelegraphStart.y +
				    runtime.LockedDirection.y * (StrikeOffset + EnemyPatternConstants::Warrior::StrikeRadius),
			};
			runtime.TelegraphWidth = EnemyPatternConstants::Warrior::StrikeRadius * 1.45f;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 strike_position{
					runtime.TelegraphStart.x + runtime.LockedDirection.x * StrikeOffset,
					runtime.TelegraphStart.y + runtime.LockedDirection.y * StrikeOffset,
				};
				SpawnWarriorStrike(strike_position, runtime.LockedDirection, runtime.Type);
				runtime.State = ActionState::WarriorRecovery;
				runtime.Timer = AttackRecoveryDuration;
			}
			break;
		case ActionState::WarriorRecovery:
			enemy.Update(delta_time, player_position, 0.18f);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::WarriorChase;
				runtime.Timer = GetRandomDuration(0.85f, 1.25f);
			}
			break;
		default:
			runtime.State = ActionState::WarriorChase;
			runtime.Timer = 0.5f;
			enemy.Update(delta_time, player_position,
			             runtime.Type == MonsterType::OrcWarrior ? ChaseSpeedScale : 1.18f);
			break;
		}
	}

	void UpdateOrc(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	               const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float ThrowMinimumRange = 240.0f;
		static constexpr float ThrowRange = 780.0f;
		switch (runtime.State)
		{
		case ActionState::OrcCooldown:
		{
			UpdateRangedMovement(enemy, delta_time, player_position);
			runtime.Timer -= delta_time;
			const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
			const float distance_squared = DistanceSquared(attack_origin, player_position);
			if (runtime.Timer <= 0.0f && distance_squared <= ThrowRange * ThrowRange &&
			    distance_squared >= ThrowMinimumRange * ThrowMinimumRange)
			{
				runtime.LockedDirection = GetDirection(attack_origin, player_position);
				runtime.TelegraphStart = attack_origin;
				runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(
				    attack_origin, runtime.LockedDirection, EnemyPatternConstants::OrcAxe::ProjectileMaxDistance,
				    EnemyPatternConstants::OrcAxe::ProjectileRadius);
				runtime.TelegraphWidth = EnemyPatternConstants::OrcAxe::ProjectileRadius * 1.25f;
				runtime.State = ActionState::OrcWindup;
				runtime.Timer = EnemyPatternConstants::OrcAxe::ThrowWindupDuration;
			}
			break;
		}
		case ActionState::OrcWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(
			    runtime.TelegraphStart, runtime.LockedDirection, EnemyPatternConstants::OrcAxe::ProjectileMaxDistance,
			    EnemyPatternConstants::OrcAxe::ProjectileRadius);
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 attack_origin = GetAttackOrigin(enemy);
				SpawnAxeProjectile(
				    {
				        attack_origin.x + runtime.LockedDirection.x * 36.0f,
				        attack_origin.y + runtime.LockedDirection.y * 36.0f,
				    },
				    runtime.LockedDirection, enemy_id);
				runtime.State = ActionState::OrcCooldown;
				runtime.Timer = GetRandomDuration(2.1f, 3.0f);
			}
			break;
		default:
			runtime.State = ActionState::OrcCooldown;
			runtime.Timer = 1.0f;
			enemy.Update(delta_time, player_position, 0.8f);
			break;
		}
	}

	void UpdateRogue(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float StrikeOffset = 72.0f;
		static constexpr float DashSpeed = 720.0f;
		static constexpr float TriggerRange = 340.0f;
		switch (runtime.State)
		{
		case ActionState::RogueApproach:
		{
			enemy.Update(delta_time, player_position, 1.30f);
			runtime.Timer -= delta_time;
			const DirectX::XMFLOAT2 origin = GetAttackOrigin(enemy);
			const float distance = Distance(origin, player_position);
			if (runtime.Timer <= 0.0f && distance <= TriggerRange)
			{
				runtime.LockedDirection = GetDirection(origin, player_position);
				runtime.DashDistance = std::max(0.0f, distance - StrikeOffset);
				runtime.TelegraphStart = origin;
				runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(
				    origin, runtime.LockedDirection, runtime.DashDistance + StrikeOffset, enemy.GetCollisionRadius());
				runtime.TelegraphWidth = EnemyPatternConstants::Rogue::StrikeRadius * 1.25f;
				runtime.State = ActionState::RogueWindup;
				runtime.Timer = EnemyPatternConstants::Rogue::WindupDuration;
			}
			break;
		}
		case ActionState::RogueWindup:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.TelegraphStart = GetAttackOrigin(enemy);
			runtime.TelegraphEnd =
			    ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
			                                       runtime.DashDistance + StrikeOffset, enemy.GetCollisionRadius());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 dash_end = ProceduralMap_TraceWalkableSegment(
				    runtime.TelegraphStart, runtime.LockedDirection, runtime.DashDistance, enemy.GetCollisionRadius());
				runtime.State = ActionState::RogueDash;
				runtime.Timer = Distance(runtime.TelegraphStart, dash_end) / DashSpeed;
			}
			break;
		case ActionState::RogueDash:
		{
			enemy.Update(delta_time, player_position, 0.0f);
			const float movement_time = std::min(delta_time, std::max(runtime.Timer, 0.0f));
			enemy.ApplySeparation({
			    runtime.LockedDirection.x * DashSpeed * movement_time,
			    runtime.LockedDirection.y * DashSpeed * movement_time,
			});
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const DirectX::XMFLOAT2 origin = GetAttackOrigin(enemy);
				SpawnRogueStrike(
				    {
				        origin.x + runtime.LockedDirection.x * StrikeOffset,
				        origin.y + runtime.LockedDirection.y * StrikeOffset,
				    },
				    runtime.LockedDirection);
				runtime.State = ActionState::RogueApproach;
				runtime.Timer = GetRandomDuration(2.2f, 3.2f);
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
} // namespace EnemyAttackPattern::Internal
