#include "projectile.h"

#include <array>

#include "game_enemy.h"
#include "indexed_slot_pool.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "trail.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

static std::array<cProjectile, PROJECTILE_MAX> g_Projectiles{};
static IndexedSlotPool<PROJECTILE_MAX, PROJECTILE_INVALID_ID> g_ProjectileSlots;
static int g_MagicBladeLaunchEvents = 0;

static void Projectile_ResetTargetHits(cProjectile& projectile)
{
	projectile.TargetHitCount = 0;
	projectile.HitTargetIDs.fill(PROJECTILE_INVALID_ID);
}

static void Projectile_BeginBoomerangReturn(cProjectile& projectile)
{
	if (projectile.IsReturningToOwner)
	{
		return;
	}
	projectile.IsReturningToOwner = true;
	Projectile_ResetTargetHits(projectile);
}

static bool Projectile_BuildBezierHomingSegment(cProjectile& projectile)
{
	DirectX::XMFLOAT2 target{};
	int target_id = PROJECTILE_INVALID_ID;
	if (!GameEnemy::FindNearestAliveForChain(projectile.Position, nullptr, 0, 1000000.0f, target_id, target))
	{
		return false;
	}

	const DirectX::XMFLOAT2 to_target = {
		target.x - projectile.Position.x,
		target.y - projectile.Position.y,
	};
	const float target_distance_sq = LengthSquared(to_target);
	if (target_distance_sq <= 0.0001f || projectile.BezierSpeed <= 0.0001f)
	{
		return false;
	}

	const float target_distance = std::sqrt(target_distance_sq);
	const DirectX::XMFLOAT2 target_direction = NormalizeOr(to_target, { 1.0f, 0.0f });
	const DirectX::XMFLOAT2 forward = NormalizeOr(projectile.Velocity, target_direction);
	const float lateral_offset =
	    std::min(projectile.BezierCurveStrength, target_distance * 0.32f) * projectile.BezierCurveDirection;
	const DirectX::XMFLOAT2 perpendicular = { -forward.y, forward.x };

	projectile.BezierTargetID = target_id;
	projectile.BezierStart = projectile.Position;
	projectile.BezierEnd = target;
	projectile.BezierControl = {
		projectile.Position.x + forward.x * target_distance * 0.28f + perpendicular.x * lateral_offset,
		projectile.Position.y + forward.y * target_distance * 0.28f + perpendicular.y * lateral_offset,
	};
	projectile.BezierControl2 = {
		target.x - target_direction.x * target_distance * 0.18f + perpendicular.x * lateral_offset * 0.62f,
		target.y - target_direction.y * target_distance * 0.18f + perpendicular.y * lateral_offset * 0.62f,
	};
	projectile.BezierElapsed = 0.0f;
	projectile.BezierDuration =
	    std::max((target_distance + std::abs(lateral_offset) * 0.55f) / projectile.BezierSpeed, 0.08f);
	projectile.HasBezierSegment = true;
	return true;
}

static DirectX::XMFLOAT2 Projectile_GetNextPosition(cProjectile& projectile, float delta_time,
                                                    const DirectX::XMFLOAT2& owner_position)
{
	if (projectile.MotionBehavior == ProjectileMotionBehavior::OrbitOwner)
	{
		const float angle = projectile.OrbitPhase + projectile.Age * projectile.OrbitAngularSpeed;
		const DirectX::XMFLOAT2 next_position = {
			owner_position.x + std::cos(angle) * projectile.OrbitRadius,
			owner_position.y + std::sin(angle) * projectile.OrbitRadius,
		};
		if (delta_time > 0.0001f)
		{
			projectile.Velocity = {
				(next_position.x - projectile.Position.x) / delta_time,
				(next_position.y - projectile.Position.y) / delta_time,
			};
		}
		projectile.Rotation += projectile.SpinSpeed * delta_time;
		return next_position;
	}

	if (projectile.MotionBehavior == ProjectileMotionBehavior::Boomerang)
	{
		if (!projectile.IsReturningToOwner && projectile.Age >= projectile.BoomerangReturnTime)
		{
			Projectile_BeginBoomerangReturn(projectile);
		}
		projectile.Rotation += projectile.SpinSpeed * delta_time;
		if (!projectile.IsReturningToOwner)
		{
			return {
				projectile.Position.x + projectile.Velocity.x * delta_time,
				projectile.Position.y + projectile.Velocity.y * delta_time,
			};
		}

		const DirectX::XMFLOAT2 to_owner = {
			owner_position.x - projectile.Position.x,
			owner_position.y - projectile.Position.y,
		};
		const float distance_sq = LengthSquared(to_owner);
		const float speed = Length(projectile.Velocity);
		const float return_step = speed * delta_time;
		const float catch_radius = std::max(projectile.Radius * 1.5f, return_step);
		if (distance_sq <= catch_radius * catch_radius)
		{
			projectile.HasReachedOwner = true;
			return owner_position;
		}

		const DirectX::XMFLOAT2 return_direction = NormalizeOr(to_owner, { 0.0f, 0.0f });
		projectile.Velocity = {
			return_direction.x * speed,
			return_direction.y * speed,
		};
		return {
			projectile.Position.x + projectile.Velocity.x * delta_time,
			projectile.Position.y + projectile.Velocity.y * delta_time,
		};
	}

	if (projectile.MotionBehavior == ProjectileMotionBehavior::MagicBlade)
	{
		const float launch_speed = Length(projectile.MagicBladeLaunchVelocity);
		if (launch_speed <= 0.0001f)
		{
			return projectile.Position;
		}

		const DirectX::XMFLOAT2 forward = NormalizeOr(projectile.MagicBladeLaunchVelocity, { 1.0f, 0.0f });
		const float side = projectile.BezierCurveDirection;
		const DirectX::XMFLOAT2 perpendicular = {
			-forward.y * side,
			forward.x * side,
		};
		const DirectX::XMFLOAT2 ready_position = {
			owner_position.x + perpendicular.x * projectile.MagicBladeSideOffset - forward.x * 8.0f,
			owner_position.y + perpendicular.y * projectile.MagicBladeSideOffset - forward.y * 8.0f,
		};

		if (projectile.Age < projectile.MagicBladeSummonTime)
		{
			const float t = Saturate(projectile.Age / projectile.MagicBladeSummonTime);
			const float inverse_t = 1.0f - t;
			const DirectX::XMFLOAT2 control1 = {
				projectile.MagicBladeSummonStart.x - forward.x * 72.0f + perpendicular.x * 58.0f,
				projectile.MagicBladeSummonStart.y - forward.y * 72.0f + perpendicular.y * 58.0f,
			};
			const DirectX::XMFLOAT2 control2 = {
				ready_position.x - forward.x * 78.0f - perpendicular.x * 22.0f,
				ready_position.y - forward.y * 78.0f - perpendicular.y * 22.0f,
			};
			const DirectX::XMFLOAT2 next_position = {
				inverse_t * inverse_t * inverse_t * projectile.MagicBladeSummonStart.x +
				    3.0f * inverse_t * inverse_t * t * control1.x + 3.0f * inverse_t * t * t * control2.x +
				    t * t * t * ready_position.x,
				inverse_t * inverse_t * inverse_t * projectile.MagicBladeSummonStart.y +
				    3.0f * inverse_t * inverse_t * t * control1.y + 3.0f * inverse_t * t * t * control2.y +
				    t * t * t * ready_position.y,
			};
			if (delta_time > 0.0001f)
			{
				projectile.Velocity = {
					(next_position.x - projectile.Position.x) / delta_time,
					(next_position.y - projectile.Position.y) / delta_time,
				};
				projectile.Rotation = std::atan2(projectile.Velocity.x, -projectile.Velocity.y);
			}
			return next_position;
		}

		if (projectile.Age < projectile.MagicBladeSummonTime + projectile.MagicBladeReadyDelay)
		{
			projectile.Velocity = { 0.0f, 0.0f };
			projectile.Rotation = std::atan2(forward.x, -forward.y);
			return ready_position;
		}

		if (!projectile.MagicBladeHasLaunched)
		{
			projectile.MagicBladeHasLaunched = true;
			++g_MagicBladeLaunchEvents;
			projectile.CanHitTargets = true;
			projectile.Velocity = projectile.MagicBladeLaunchVelocity;

			DirectX::XMFLOAT2 aimed_target{};
			int aimed_target_id = PROJECTILE_INVALID_ID;
			if (GameEnemy::FindNearestAliveForChain(owner_position, nullptr, 0, 1000000.0f, aimed_target_id,
			                                        aimed_target))
			{
				const DirectX::XMFLOAT2 to_target = {
					aimed_target.x - projectile.Position.x,
					aimed_target.y - projectile.Position.y,
				};
				const float target_distance_sq = LengthSquared(to_target);
				if (target_distance_sq > 0.0001f)
				{
					const DirectX::XMFLOAT2 target_direction = NormalizeOr(to_target, { 0.0f, 0.0f });
					projectile.Velocity = {
						target_direction.x * launch_speed,
						target_direction.y * launch_speed,
					};
				}
			}
			projectile.Rotation = std::atan2(projectile.Velocity.x, -projectile.Velocity.y);
			Projectile_ResetTargetHits(projectile);
		}
		return {
			projectile.Position.x + projectile.Velocity.x * delta_time,
			projectile.Position.y + projectile.Velocity.y * delta_time,
		};
	}

	if (projectile.UsesBezierHoming && projectile.HasBezierSegment)
	{
		DirectX::XMFLOAT2 moving_target{};
		if (GameEnemy::TryGetAlivePosition(projectile.BezierTargetID, moving_target))
		{
			const DirectX::XMFLOAT2 target_movement = {
				moving_target.x - projectile.BezierEnd.x,
				moving_target.y - projectile.BezierEnd.y,
			};
			projectile.BezierControl2.x += target_movement.x;
			projectile.BezierControl2.y += target_movement.y;
			projectile.BezierEnd = moving_target;
		}
		else
		{
			projectile.HasBezierSegment = false;
			projectile.UsesBezierHoming = false;
			projectile.BezierTargetID = PROJECTILE_INVALID_ID;
		}
	}
	if (projectile.UsesBezierHoming && !projectile.HasBezierSegment)
	{
		Projectile_BuildBezierHomingSegment(projectile);
	}
	if (!projectile.UsesBezierHoming || !projectile.HasBezierSegment)
	{
		return {
			projectile.Position.x + projectile.Velocity.x * delta_time,
			projectile.Position.y + projectile.Velocity.y * delta_time,
		};
	}

	projectile.BezierElapsed += delta_time;
	const float t = Saturate(projectile.BezierElapsed / projectile.BezierDuration);
	const float inverse_t = 1.0f - t;
	const DirectX::XMFLOAT2 next_position = {
		inverse_t * inverse_t * inverse_t * projectile.BezierStart.x +
		    3.0f * inverse_t * inverse_t * t * projectile.BezierControl.x +
		    3.0f * inverse_t * t * t * projectile.BezierControl2.x + t * t * t * projectile.BezierEnd.x,
		inverse_t * inverse_t * inverse_t * projectile.BezierStart.y +
		    3.0f * inverse_t * inverse_t * t * projectile.BezierControl.y +
		    3.0f * inverse_t * t * t * projectile.BezierControl2.y + t * t * t * projectile.BezierEnd.y,
	};
	if (delta_time > 0.0001f)
	{
		projectile.Velocity = {
			(next_position.x - projectile.Position.x) / delta_time,
			(next_position.y - projectile.Position.y) / delta_time,
		};
		projectile.Rotation = std::atan2(projectile.Velocity.x, -projectile.Velocity.y);
	}
	if (t >= 1.0f)
	{
		projectile.HasBezierSegment = false;
		projectile.UsesBezierHoming = false;
	}
	return next_position;
}

static DirectX::XMFLOAT2 Projectile_FindLastWalkablePosition(const DirectX::XMFLOAT2& start,
                                                             const DirectX::XMFLOAT2& end, float radius,
                                                             float& out_travel_amount)
{
	float walkable_amount = 0.0f;
	float blocked_amount = 1.0f;
	for (int iteration = 0; iteration < 8; ++iteration)
	{
		const float amount = (walkable_amount + blocked_amount) * 0.5f;
		const DirectX::XMFLOAT2 candidate = {
			start.x + (end.x - start.x) * amount,
			start.y + (end.y - start.y) * amount,
		};
		if (ProceduralMap_IsSegmentWalkable(start, candidate, radius))
		{
			walkable_amount = amount;
		}
		else
		{
			blocked_amount = amount;
		}
	}

	out_travel_amount = walkable_amount;
	return {
		start.x + (end.x - start.x) * walkable_amount,
		start.y + (end.y - start.y) * walkable_amount,
	};
}

static bool Projectile_TryAimBounceAtEnemy(cProjectile& projectile, const DirectX::XMFLOAT2& impact_position,
                                           float remaining_time)
{
	static constexpr float TWO_PI = 6.28318530718f;
	static constexpr float MAX_CORRECTION_ANGLE = 1.309f;
	static constexpr float AIM_RANGE = 900.0f;

	// 75도.

	if (projectile.Layer != CollisionLayer::PlayerBullet)
	{
		return false;
	}

	DirectX::XMFLOAT2 target_position{};
	int target_id = PROJECTILE_INVALID_ID;
	if (!GameEnemy::FindNearestAliveForChain(impact_position, projectile.HitTargetIDs.data(), projectile.TargetHitCount,
	                                         AIM_RANGE, target_id, target_position))
	{
		return false;
	}

	const float speed_sq = LengthSquared(projectile.Velocity);
	const DirectX::XMFLOAT2 to_target = {
		target_position.x - impact_position.x,
		target_position.y - impact_position.y,
	};
	const float target_distance_sq = LengthSquared(to_target);
	if (speed_sq <= 0.0001f || target_distance_sq <= 0.0001f)
	{
		return false;
	}

	const float speed = std::sqrt(speed_sq);
	const float reflected_angle = std::atan2(projectile.Velocity.y, projectile.Velocity.x);
	const float target_angle = std::atan2(to_target.y, to_target.x);
	const float angle_delta = std::remainder(target_angle - reflected_angle, TWO_PI);
	const float corrected_angle =
	    reflected_angle + std::clamp(angle_delta, -MAX_CORRECTION_ANGLE, MAX_CORRECTION_ANGLE);
	const DirectX::XMFLOAT2 corrected_velocity = {
		std::cos(corrected_angle) * speed,
		std::sin(corrected_angle) * speed,
	};

	// 조준 보정으로 투사체가 방금 부딪힌 벽을 다시 향하지 않도록 한다.
	const float probe_time = std::max(remaining_time, (projectile.Radius * 2.0f + 4.0f) / speed);
	const DirectX::XMFLOAT2 probe_position = {
		impact_position.x + corrected_velocity.x * probe_time,
		impact_position.y + corrected_velocity.y * probe_time,
	};
	if (!ProceduralMap_IsSegmentWalkable(impact_position, probe_position, projectile.Radius))
	{
		return false;
	}

	projectile.Velocity = corrected_velocity;
	return true;
}

static bool Projectile_TryBounceOffMap(cProjectile& projectile, const DirectX::XMFLOAT2& previous_position,
                                       const DirectX::XMFLOAT2& blocked_position, float delta_time)
{
	if (projectile.RemainingBounces <= 0)
	{
		return false;
	}

	const DirectX::XMFLOAT2 movement = {
		blocked_position.x - previous_position.x,
		blocked_position.y - previous_position.y,
	};
	const DirectX::XMFLOAT2 x_destination = {
		blocked_position.x,
		previous_position.y,
	};
	const DirectX::XMFLOAT2 y_destination = {
		previous_position.x,
		blocked_position.y,
	};
	bool reflect_x = std::abs(movement.x) > 0.0001f &&
	                 !ProceduralMap_IsSegmentWalkable(previous_position, x_destination, projectile.Radius);
	bool reflect_y = std::abs(movement.y) > 0.0001f &&
	                 !ProceduralMap_IsSegmentWalkable(previous_position, y_destination, projectile.Radius);

	// 대각선 방향에서만 충돌하면 모서리에 맞은 것이므로 두 축을 모두 반전한다.
	if (!reflect_x && !reflect_y)
	{
		reflect_x = true;
		reflect_y = true;
	}

	--projectile.RemainingBounces;
	if (projectile.RemainingBounces <= 0)
	{
		return false;
	}

	float travel_amount = 0.0f;
	const DirectX::XMFLOAT2 impact_position =
	    Projectile_FindLastWalkablePosition(previous_position, blocked_position, projectile.Radius, travel_amount);
	if (reflect_x)
	{
		projectile.Velocity.x = -projectile.Velocity.x;
	}
	if (reflect_y)
	{
		projectile.Velocity.y = -projectile.Velocity.y;
	}

	const float remaining_time = delta_time * (1.0f - travel_amount);
	Projectile_TryAimBounceAtEnemy(projectile, impact_position, remaining_time);
	projectile.Rotation = std::atan2(projectile.Velocity.x, -projectile.Velocity.y);
	const DirectX::XMFLOAT2 reflected_position = {
		impact_position.x + projectile.Velocity.x * remaining_time,
		impact_position.y + projectile.Velocity.y * remaining_time,
	};
	projectile.Position = ProceduralMap_IsSegmentWalkable(impact_position, reflected_position, projectile.Radius)
	                          ? reflected_position
	                          : impact_position;
	return true;
}

static float Projectile_GetTrailPixelBlockSize(const cProjectile& projectile)
{
	const float width = projectile.TrailWidth > 0.0f ? projectile.TrailWidth : projectile.Width * 0.65f;
	return std::clamp(std::round(width * 0.3f * 0.5f) * 2.0f, 6.0f, 14.0f);
}

static void Projectile_EmitTrailSample(const cProjectile& projectile, const DirectX::XMFLOAT2& position)
{
	const float pixel_block_size = Projectile_GetTrailPixelBlockSize(projectile);

	cTrailDesc trail_desc{};
	// 잔상은 투사체 중심에서 시작한다. 투사체 스프라이트를 그 위에 그려
	// 잔상이 투사체 바로 아래에서 이어져 나오도록 한다.
	trail_desc.Position = position;
	trail_desc.Width = pixel_block_size;
	trail_desc.Height = pixel_block_size;
	trail_desc.StartScale = projectile.TrailStartScale;
	trail_desc.EndScale = projectile.TrailEndScale;
	// 블록을 화면 축에 맞춰 가장자리가 픽셀 격자에 놓이도록 한다.
	// 사각형을 회전하면 대각선 가장자리가 다시 부드러워진다.
	trail_desc.Rotation = 0.0f;
	trail_desc.LifeTime = projectile.TrailLifeTime;
	trail_desc.TextureID =
	    projectile.TrailTextureID != TEXTURE_INVALID_ID ? projectile.TrailTextureID : projectile.TextureID;
	trail_desc.Color = projectile.TrailColor;
	trail_desc.Pixelated = true;
	trail_desc.PixelGridSize = 2.0f;
	trail_desc.FadeSteps = 5;
	TrailSystem_Emit(trail_desc);
}

static void Projectile_EmitTrail(cProjectile& projectile, const DirectX::XMFLOAT2& previous_position,
                                 const DirectX::XMFLOAT2& current_position)
{
	static constexpr int MAX_EMITS_PER_UPDATE = 32;

	if (!projectile.UsesTrail)
	{
		return;
	}

	const float dx = current_position.x - previous_position.x;
	const float dy = current_position.y - previous_position.y;
	const float segment_length = Distance(previous_position, current_position);
	if (segment_length <= 0.0001f)
	{
		return;
	}

	const DirectX::XMFLOAT2 move_dir = {
		dx / segment_length,
		dy / segment_length,
	};
	const float speed = Length(projectile.Velocity);
	const float configured_spacing = speed * projectile.TrailEmitInterval;
	const float connected_spacing = Projectile_GetTrailPixelBlockSize(projectile) * 0.65f;
	const float emit_spacing = std::max(std::min(configured_spacing, connected_spacing), 3.0f);

	float distance_along_segment = emit_spacing - projectile.TrailEmitDistance;

	int emit_count = 0;
	while (distance_along_segment <= segment_length && emit_count < MAX_EMITS_PER_UPDATE)
	{
		const DirectX::XMFLOAT2 sample_position = {
			previous_position.x + move_dir.x * distance_along_segment,
			previous_position.y + move_dir.y * distance_along_segment,
		};
		Projectile_EmitTrailSample(projectile, sample_position);
		distance_along_segment += emit_spacing;
		++emit_count;
	}

	projectile.TrailEmitDistance = std::fmod(projectile.TrailEmitDistance + segment_length, emit_spacing);
}

void ProjectileSystem_Initialize()
{
	TrailSystem_Initialize();

	for (int i = 0; i < PROJECTILE_MAX; ++i)
	{
		g_Projectiles[i] = cProjectile{};
	}

	g_ProjectileSlots.Reset();
	g_MagicBladeLaunchEvents = 0;
}

void ProjectileSystem_Finalize()
{
	ProjectileSystem_Clear();
	TrailSystem_Finalize();
}

void ProjectileSystem_Clear()
{
	while (g_ProjectileSlots.GetActiveCount() > 0)
	{
		ProjectileSystem_Deactivate(g_ProjectileSlots.GetActiveID(g_ProjectileSlots.GetActiveCount() - 1));
	}

	TrailSystem_Clear();
	g_MagicBladeLaunchEvents = 0;
}

int ProjectileSystem_Fire(const cProjectileDesc& desc)
{
	if (!g_ProjectileSlots.HasFreeSlot())
	{
		return PROJECTILE_INVALID_ID;
	}
	if (desc.AliveLimitGroup != PROJECTILE_INVALID_ID && desc.MaxAliveInGroup > 0)
	{
		int alive_in_group = 0;
		for (int i = 0; i < g_ProjectileSlots.GetActiveCount(); ++i)
		{
			const cProjectile& active_projectile = g_Projectiles[g_ProjectileSlots.GetActiveID(i)];
			if (active_projectile.AliveLimitGroup == desc.AliveLimitGroup)
			{
				++alive_in_group;
			}
		}
		if (alive_in_group >= desc.MaxAliveInGroup)
		{
			return PROJECTILE_INVALID_ID;
		}
	}

	const int projectile_id = g_ProjectileSlots.Acquire();
	if (projectile_id == PROJECTILE_INVALID_ID)
	{
		return PROJECTILE_INVALID_ID;
	}

	cProjectile& projectile = g_Projectiles[projectile_id];
	projectile.IsActive = true;
	projectile.Position = desc.Position;
	projectile.Velocity = desc.Velocity;
	projectile.Radius = PositiveOr(desc.Radius, 1.0f);
	projectile.Width = PositiveOr(desc.Width, projectile.Radius * 2.0f);
	projectile.Height = PositiveOr(desc.Height, projectile.Radius * 2.0f);
	projectile.Rotation = desc.Rotation;
	projectile.Damage = desc.Damage;
	projectile.Age = 0.0f;
	projectile.LifeTime = std::max(desc.LifeTime, 0.0f);
	projectile.TextureID = desc.TextureID;
	projectile.OwnerID = desc.OwnerID;
	projectile.Layer = desc.Layer;
	projectile.HitMask = desc.HitMask;
	projectile.HitBehavior = desc.HitBehavior;
	projectile.AreaRadius = std::max(desc.AreaRadius, 0.0f);
	projectile.MaxTargetHits = std::clamp(desc.MaxTargetHits, 1, PROJECTILE_HIT_HISTORY_MAX);
	projectile.TargetHitCount = 0;
	projectile.HitTargetIDs.fill(PROJECTILE_INVALID_ID);
	projectile.RemainingBounces = std::max(desc.MaxBounces, 0);
	projectile.AliveLimitGroup = desc.AliveLimitGroup;
	projectile.UsesBezierHoming = desc.UsesBezierHoming;
	projectile.BezierCurveStrength = std::max(desc.BezierCurveStrength, 0.0f);
	projectile.BezierCurveDirection = desc.BezierCurveDirection < 0.0f ? -1.0f : 1.0f;
	projectile.BezierSpeed = Length(desc.Velocity);
	projectile.BezierTargetID = PROJECTILE_INVALID_ID;
	projectile.HasBezierSegment = false;
	projectile.MotionBehavior = desc.MotionBehavior;
	projectile.BoomerangReturnTime = std::max(desc.BoomerangReturnTime, 0.0f);
	projectile.IsReturningToOwner = false;
	projectile.HasReachedOwner = false;
	projectile.OrbitRadius = std::max(desc.OrbitRadius, 0.0f);
	projectile.OrbitAngularSpeed = desc.OrbitAngularSpeed;
	projectile.OrbitPhase = desc.OrbitPhase;
	projectile.SpinSpeed = desc.SpinSpeed;
	projectile.RepeatHitInterval = std::max(desc.RepeatHitInterval, 0.0f);
	projectile.RepeatHitTimer = 0.0f;
	projectile.MagicBladeSummonTime = std::max(desc.MagicBladeSummonTime, 0.0f);
	projectile.MagicBladeReadyDelay = std::max(desc.MagicBladeReadyDelay, 0.0f);
	projectile.MagicBladeSideOffset = std::max(desc.MagicBladeSideOffset, 0.0f);
	projectile.MagicBladeSummonStart = desc.Position;
	projectile.MagicBladeLaunchVelocity = desc.Velocity;
	projectile.MagicBladeHasLaunched = false;
	projectile.CanHitTargets = desc.MotionBehavior != ProjectileMotionBehavior::MagicBlade;
	if (desc.MotionBehavior == ProjectileMotionBehavior::MagicBlade)
	{
		projectile.Velocity = { 0.0f, 0.0f };
	}
	projectile.UsesTrail = desc.UsesTrail;
	projectile.TrailTextureID = desc.TrailTextureID;
	projectile.TrailEmitInterval = PositiveOr(desc.TrailEmitInterval, 0.02f);
	projectile.TrailEmitDistance = 0.0f;
	projectile.TrailWidth = std::max(desc.TrailWidth, 0.0f);
	projectile.TrailLifeTime = PositiveOr(desc.TrailLifeTime, 0.01f);
	projectile.TrailStartScale = PositiveOr(desc.TrailStartScale, 1.0f);
	projectile.TrailEndScale = std::max(desc.TrailEndScale, 0.0f);
	projectile.TrailColor = desc.TrailColor;

	return projectile_id;
}

void ProjectileSystem_Update(float delta_time, const DirectX::XMFLOAT2& owner_position)
{
	delta_time = std::max(delta_time, 0.0f);
	TrailSystem_Update(delta_time);

	for (int active_index = 0; active_index < g_ProjectileSlots.GetActiveCount();)
	{
		const int projectile_id = g_ProjectileSlots.GetActiveID(active_index);
		cProjectile& projectile = g_Projectiles[projectile_id];

		projectile.Age += delta_time;
		if (projectile.RepeatHitInterval > 0.0f)
		{
			projectile.RepeatHitTimer += delta_time;
			if (projectile.RepeatHitTimer >= projectile.RepeatHitInterval)
			{
				projectile.RepeatHitTimer = std::fmod(projectile.RepeatHitTimer, projectile.RepeatHitInterval);
				Projectile_ResetTargetHits(projectile);
			}
		}
		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position = Projectile_GetNextPosition(projectile, delta_time, owner_position);
		if (projectile.HasReachedOwner)
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}

		const bool ignores_map =
		    projectile.MotionBehavior == ProjectileMotionBehavior::OrbitOwner ||
		    (projectile.MotionBehavior == ProjectileMotionBehavior::MagicBlade && !projectile.MagicBladeHasLaunched) ||
		    (projectile.MotionBehavior == ProjectileMotionBehavior::Boomerang && projectile.IsReturningToOwner);
		const bool hit_map =
		    !ignores_map && !ProceduralMap_IsSegmentWalkable(previous_position, next_position, projectile.Radius);
		if (hit_map && projectile.MotionBehavior == ProjectileMotionBehavior::Boomerang)
		{
			Projectile_BeginBoomerangReturn(projectile);
			projectile.Position = previous_position;
		}
		else if (hit_map && !Projectile_TryBounceOffMap(projectile, previous_position, next_position, delta_time))
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}
		else if (!hit_map)
		{
			projectile.Position = next_position;
		}
		Projectile_EmitTrail(projectile, previous_position, projectile.Position);

		const bool is_life_over = projectile.LifeTime > 0.0f && projectile.Age >= projectile.LifeTime;
		if (is_life_over)
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}

		++active_index;
	}
}

void ProjectileSystem_Draw()
{
	TrailSystem_Draw();
	static std::unordered_map<int, std::vector<SpriteInstance>> batches;
	static std::unordered_map<int, std::vector<SpriteInstance>> enemy_batches;
	for (auto& batch : batches)
	{
		batch.second.clear();
	}
	for (auto& batch : enemy_batches)
	{
		batch.second.clear();
	}

	for (int i = 0; i < g_ProjectileSlots.GetActiveCount(); ++i)
	{
		const cProjectile& projectile = g_Projectiles[g_ProjectileSlots.GetActiveID(i)];
		if (projectile.TextureID == TEXTURE_INVALID_ID)
		{
			continue;
		}

		auto& projectile_batches = projectile.Layer == CollisionLayer::EnemyBullet ? enemy_batches : batches;
		projectile_batches[projectile.TextureID].push_back({
		    projectile.Position,
		    { projectile.Width, projectile.Height },
		    projectile.Rotation,
		    { 1.0f, 1.0f, 1.0f, 1.0f },
		});
	}

	for (const auto& [texture_id, instances] : batches)
	{
		if (!instances.empty())
		{
			SpriteInstanced_Draw(texture_id, instances.data(), static_cast<int>(instances.size()));
		}
	}

	for (const auto& [texture_id, instances] : enemy_batches)
	{
		if (!instances.empty())
		{
			SpriteInstanced_DrawOutlinedUnlit(texture_id, instances.data(), static_cast<int>(instances.size()),
			                                  { 1.0f, 0.035f, 0.015f, 0.96f }, 1.0f);
		}
	}
}

void ProjectileSystem_RegisterColliders()
{
	for (int i = 0; i < g_ProjectileSlots.GetActiveCount(); ++i)
	{
		const int projectile_id = g_ProjectileSlots.GetActiveID(i);
		const cProjectile& projectile = g_Projectiles[projectile_id];
		CollisionSystem_RegisterCircle(projectile_id, projectile.Layer, projectile.HitMask, projectile.Position,
		                               projectile.Radius, projectile.IsActive && projectile.CanHitTargets);
	}
}

void ProjectileSystem_Deactivate(int projectile_id)
{
	if (!g_ProjectileSlots.IsActive(projectile_id) || !g_Projectiles[projectile_id].IsActive)
	{
		return;
	}

	g_Projectiles[projectile_id] = cProjectile{};
	g_ProjectileSlots.Release(projectile_id);
}

void ProjectileSystem_DeactivateGroup(int alive_limit_group)
{
	if (alive_limit_group == PROJECTILE_INVALID_ID)
	{
		return;
	}
	for (int active_index = 0; active_index < g_ProjectileSlots.GetActiveCount();)
	{
		const int projectile_id = g_ProjectileSlots.GetActiveID(active_index);
		if (g_Projectiles[projectile_id].AliveLimitGroup == alive_limit_group)
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}
		++active_index;
	}
}

int ProjectileSystem_GetActiveGroupCount(int alive_limit_group)
{
	if (alive_limit_group == PROJECTILE_INVALID_ID)
	{
		return 0;
	}
	int active_count = 0;
	for (int i = 0; i < g_ProjectileSlots.GetActiveCount(); ++i)
	{
		const cProjectile& projectile = g_Projectiles[g_ProjectileSlots.GetActiveID(i)];
		if (projectile.AliveLimitGroup == alive_limit_group)
		{
			++active_count;
		}
	}
	return active_count;
}

bool ProjectileSystem_IsActive(int projectile_id)
{
	return g_ProjectileSlots.IsActive(projectile_id) && g_Projectiles[projectile_id].IsActive;
}

const cProjectile* ProjectileSystem_GetProjectile(int projectile_id)
{
	if (!ProjectileSystem_IsActive(projectile_id))
	{
		return nullptr;
	}

	return &g_Projectiles[projectile_id];
}

bool ProjectileSystem_TryRegisterTargetHit(int projectile_id, int target_id)
{
	if (!ProjectileSystem_IsActive(projectile_id) || target_id < 0 || !g_Projectiles[projectile_id].CanHitTargets)
	{
		return false;
	}

	cProjectile& projectile = g_Projectiles[projectile_id];
	for (int i = 0; i < projectile.TargetHitCount; ++i)
	{
		if (projectile.HitTargetIDs[i] == target_id)
		{
			return false;
		}
	}

	if (projectile.TargetHitCount >= projectile.MaxTargetHits ||
	    projectile.TargetHitCount >= PROJECTILE_HIT_HISTORY_MAX)
	{
		return false;
	}

	projectile.HitTargetIDs[projectile.TargetHitCount++] = target_id;
	return true;
}

int ProjectileSystem_GetActiveCount()
{
	return g_ProjectileSlots.GetActiveCount();
}

int ProjectileSystem_GetCapacity()
{
	return g_ProjectileSlots.GetCapacity();
}

int ProjectileSystem_ConsumeMagicBladeLaunchEvents()
{
	const int event_count = g_MagicBladeLaunchEvents;
	g_MagicBladeLaunchEvents = 0;
	return event_count;
}
