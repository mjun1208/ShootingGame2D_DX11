#include "enemy_attack_pattern_internal.h"
#include "Constants/enemy_pattern_constants.h"

namespace EnemyAttackPattern::Internal
{
	// 크툴루 보스의 이동, 순간이동, 돌진 패턴.
	void PlayCthulhuWarpEffect(const DirectX::XMFLOAT2& position)
	{
		cGameEffectManager::GetInstance().Play(GameEffectType::SmokePoof, position, 1.45f,
		                                       { 0.28f, 0.04f, 0.72f, 0.94f });
	}

	void EmitCthulhuDashTrail(const DirectX::XMFLOAT2& start, const DirectX::XMFLOAT2& end)
	{
		if (g_WarningTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		const float dx = end.x - start.x;
		const float dy = end.y - start.y;
		const float distance = Distance(start, end);
		if (distance <= 0.5f)
		{
			return;
		}
		const float rotation = std::atan2(dy, dx);
		const DirectX::XMFLOAT2 center{
			(start.x + end.x) * 0.5f,
			(start.y + end.y) * 0.5f,
		};
		cTrailDesc ribbon{};
		ribbon.Position = center;
		ribbon.Width = distance + 150.0f;
		ribbon.Height = 132.0f;
		ribbon.StartScale = 1.0f;
		ribbon.EndScale = 0.72f;
		ribbon.Rotation = rotation;
		ribbon.LifeTime = 0.18f;
		ribbon.TextureID = g_WarningTextureID;
		ribbon.Color = { 0.08f, 0.72f, 0.60f, 0.22f };
		TrailSystem_Emit(ribbon);
		for (int streak_index = -1; streak_index <= 1; ++streak_index)
		{
			cTrailDesc streak = ribbon;
			streak.Position.y += static_cast<float>(streak_index) * 42.0f;
			streak.Width = distance + 105.0f + static_cast<float>(std::abs(streak_index)) * 34.0f;
			streak.Height = streak_index == 0 ? 18.0f : 8.0f;
			streak.EndScale = 0.18f;
			streak.LifeTime = streak_index == 0 ? 0.14f : 0.11f;
			streak.Color = streak_index == 0 ? DirectX::XMFLOAT4{ 0.30f, 1.0f, 0.76f, 0.62f }
			                                 : DirectX::XMFLOAT4{ 0.45f, 0.82f, 1.0f, 0.42f };
			TrailSystem_Emit(streak);
		}
	}

	void BeginCthulhuDashWindup(EnemyPatternRuntime& runtime, cEnemy& enemy, const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float DashTelegraphWidth = 220.0f;
		static constexpr float RoomEdgePadding = 142.0f;
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(ProceduralMap_GetRoomIndexAt(player_position));
		const bool from_left = (runtime.MovementStep & 1) == 0;
		DirectX::XMFLOAT2 dash_start = enemy.GetPosition();
		float dash_end_x = dash_start.x + (from_left ? 1000.0f : -1000.0f);
		if (room)
		{
			const float minimum_x = room->WorldMin.x + RoomEdgePadding;
			const float maximum_x = room->WorldMax.x - RoomEdgePadding;
			const float minimum_y = room->WorldMin.y + RoomEdgePadding;
			const float maximum_y = room->WorldMax.y - RoomEdgePadding;
			dash_start = {
				from_left ? minimum_x : maximum_x,
				std::clamp(player_position.y, minimum_y, maximum_y),
			};
			dash_end_x = from_left ? maximum_x : minimum_x;
		}
		const DirectX::XMFLOAT2 current_position = enemy.GetPosition();
		enemy.ApplySeparation({
		    dash_start.x - current_position.x,
		    dash_start.y - current_position.y,
		});
		runtime.LockedDirection = { from_left ? 1.0f : -1.0f, 0.0f };
		runtime.TelegraphStart = GetAttackOrigin(enemy);
		runtime.DashDistance = std::abs(dash_end_x - runtime.TelegraphStart.x);
		runtime.TelegraphEnd = ProceduralMap_TraceWalkableSegment(runtime.TelegraphStart, runtime.LockedDirection,
		                                                          runtime.DashDistance, enemy.GetMapCollisionRadius());
		runtime.TelegraphWidth = DashTelegraphWidth;
		runtime.State = ActionState::CthulhuDashWindup;
		runtime.Timer = EnemyPatternConstants::Cthulhu::DashWindupDuration;
		runtime.AnimationElapsed = 0.0f;
		PlayCthulhuWarpEffect(runtime.TelegraphStart);
	}

	void UpdateCthulhu(int enemy_id, EnemyPatternRuntime& runtime, cEnemy& enemy, float delta_time,
	                   const DirectX::XMFLOAT2& player_position)
	{
		static constexpr float DashRecoveryDuration = 0.86f;
		static constexpr float DashSpeed = 1060.0f;
		static constexpr int DashPattern = 4;
		static constexpr float CastDuration = 2.35f;
		static constexpr float StrafeSpeed = 104.0f;
		switch (runtime.State)
		{
		case ActionState::CthulhuRoam:
		{
			enemy.Update(delta_time, player_position, 0.32f);
			const DirectX::XMFLOAT2 direction = GetDirection(enemy.GetPosition(), player_position);
			enemy.ApplySeparation({
			    -direction.y * StrafeSpeed * runtime.StrafeDirection * delta_time,
			    direction.x * StrafeSpeed * runtime.StrafeDirection * delta_time,
			});
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::CthulhuCast;
				runtime.Timer = CastDuration;
				runtime.AnimationElapsed = 0.0f;
			}
			break;
		}
		case ActionState::CthulhuCast:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const int completed_pattern = runtime.MovementStep % EnemyPatternConstants::Cthulhu::PatternCount;
				++runtime.MovementStep;
				runtime.State =
				    completed_pattern == DashPattern ? ActionState::CthulhuVanish : ActionState::CthulhuRecovery;
				runtime.Timer = completed_pattern == DashPattern ? EnemyPatternConstants::Cthulhu::VanishDuration
				                                                 : DashRecoveryDuration;
				runtime.AnimationElapsed = 0.0f;
				if (completed_pattern == DashPattern)
				{
					PlayCthulhuWarpEffect(GetAttackOrigin(enemy));
				}
			}
			break;
		case ActionState::CthulhuVanish:
			enemy.Update(delta_time, player_position, 0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				BeginCthulhuDashWindup(runtime, enemy, player_position);
			}
			break;
		case ActionState::CthulhuDashWindup:
			enemy.Update(delta_time,
			             {
			                 enemy.GetPosition().x + runtime.LockedDirection.x * 100.0f,
			                 enemy.GetPosition().y,
			             },
			             0.0f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				const float path_distance = Distance(runtime.TelegraphStart, runtime.TelegraphEnd);
				runtime.State = ActionState::CthulhuDash;
				runtime.Timer = path_distance / DashSpeed;
				runtime.AnimationElapsed = 0.0f;
				if (g_BatDashAudioID >= 0)
				{
					Audio_Play(g_BatDashAudioID);
				}
				cGameEffectManager::GetInstance().Play(GameEffectType::VoidImplosion, enemy.GetPosition(), 1.18f,
				                                       { 0.18f, 1.0f, 0.68f, 0.96f });
			}
			break;
		case ActionState::CthulhuDash:
		{
			enemy.Update(delta_time,
			             {
			                 enemy.GetPosition().x + runtime.LockedDirection.x * 100.0f,
			                 enemy.GetPosition().y,
			             },
			             0.0f);
			const DirectX::XMFLOAT2 previous_position = enemy.GetPosition();
			const float movement_time = std::min(delta_time, runtime.Timer);
			enemy.ApplySeparation({
			    runtime.LockedDirection.x * DashSpeed * movement_time,
			    0.0f,
			});
			runtime.AnimationElapsed += delta_time;
			EmitCthulhuDashTrail(previous_position, enemy.GetPosition());
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				cGameEffectManager::GetInstance().Play(GameEffectType::VoidImplosion, enemy.GetPosition(), 1.05f,
				                                       { 0.18f, 0.86f, 1.0f, 0.92f });
				runtime.State = ActionState::CthulhuRecovery;
				runtime.Timer = DashRecoveryDuration;
				runtime.AnimationElapsed = 0.0f;
			}
			break;
		}
		case ActionState::CthulhuRecovery:
			enemy.Update(delta_time, player_position, 0.08f);
			runtime.AnimationElapsed += delta_time;
			runtime.Timer -= delta_time;
			if (runtime.Timer <= 0.0f)
			{
				runtime.State = ActionState::CthulhuRoam;
				runtime.Timer = GetRandomDuration(EnemyPatternConstants::Cthulhu::RoamMinDuration,
				                                  EnemyPatternConstants::Cthulhu::RoamMaxDuration);
				runtime.StrafeDirection *= -1.0f;
				runtime.AnimationElapsed = 0.0f;
			}
			break;
		default:
			runtime.State = ActionState::CthulhuRoam;
			runtime.Timer = EnemyPatternConstants::Cthulhu::RoamMinDuration;
			runtime.AnimationElapsed = 0.0f;
			enemy.Update(delta_time, player_position, 0.25f);
			break;
		}
	}
} // namespace EnemyAttackPattern::Internal
