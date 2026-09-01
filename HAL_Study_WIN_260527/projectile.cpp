#include "projectile.h"

#include "game_enemy.h"
#include "procedural_map.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "trail.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

static cProjectile g_Projectiles[PROJECTILE_MAX];
static int g_ActiveProjectiles[PROJECTILE_MAX];
static int g_ActiveIndices[PROJECTILE_MAX];
static int g_FreeProjectiles[PROJECTILE_MAX];
static int g_ActiveCount = 0;
static int g_FreeCount = 0;
static int g_MagicBladeLaunchEvents = 0;

static float Projectile_ClampPositive(float value, float fallback)
{
	return value > 0.0f ? value : fallback;
}

static bool Projectile_IsValidID(int projectile_id)
{
	return projectile_id >= 0 && projectile_id < PROJECTILE_MAX;
}

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
	if (!GameEnemy::FindNearestAliveForChain(
		projectile.Position,
		nullptr,
		0,
		1000000.0f,
		target_id,
		target))
	{
		return false;
	}

	const DirectX::XMFLOAT2 to_target = {
		target.x - projectile.Position.x,
		target.y - projectile.Position.y,
	};
	const float target_distance_sq =
		to_target.x * to_target.x + to_target.y * to_target.y;
	if (target_distance_sq <= 0.0001f || projectile.BezierSpeed <= 0.0001f)
	{
		return false;
	}

	const float target_distance = std::sqrt(target_distance_sq);
	const DirectX::XMFLOAT2 target_direction = {
		to_target.x / target_distance,
		to_target.y / target_distance,
	};
	const float velocity_length_sq =
		projectile.Velocity.x * projectile.Velocity.x +
		projectile.Velocity.y * projectile.Velocity.y;
	const float velocity_length = std::sqrt(velocity_length_sq);
	const DirectX::XMFLOAT2 forward = velocity_length > 0.0001f ?
		DirectX::XMFLOAT2{
			projectile.Velocity.x / velocity_length,
			projectile.Velocity.y / velocity_length,
		} : target_direction;
	const float lateral_offset = std::min(
		projectile.BezierCurveStrength,
		target_distance * 0.32f) * projectile.BezierCurveDirection;
	const DirectX::XMFLOAT2 perpendicular = { -forward.y, forward.x };

	projectile.BezierTargetID = target_id;
	projectile.BezierStart = projectile.Position;
	projectile.BezierEnd = target;
	projectile.BezierControl = {
		projectile.Position.x + forward.x * target_distance * 0.28f +
			perpendicular.x * lateral_offset,
		projectile.Position.y + forward.y * target_distance * 0.28f +
			perpendicular.y * lateral_offset,
	};
	projectile.BezierControl2 = {
		target.x - target_direction.x * target_distance * 0.18f +
			perpendicular.x * lateral_offset * 0.62f,
		target.y - target_direction.y * target_distance * 0.18f +
			perpendicular.y * lateral_offset * 0.62f,
	};
	projectile.BezierElapsed = 0.0f;
	projectile.BezierDuration = std::max(
		(target_distance + std::abs(lateral_offset) * 0.55f) /
			projectile.BezierSpeed,
		0.08f);
	projectile.HasBezierSegment = true;
	return true;
}

static DirectX::XMFLOAT2 Projectile_GetNextPosition(
	cProjectile& projectile,
	float delta_time,
	const DirectX::XMFLOAT2& owner_position)
{
	if (projectile.MotionBehavior == ProjectileMotionBehavior::OrbitOwner)
	{
		const float angle = projectile.OrbitPhase +
			projectile.Age * projectile.OrbitAngularSpeed;
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
		if (!projectile.IsReturningToOwner &&
			projectile.Age >= projectile.BoomerangReturnTime)
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
		const float distance_sq =
			to_owner.x * to_owner.x + to_owner.y * to_owner.y;
		const float speed = std::sqrt(
			projectile.Velocity.x * projectile.Velocity.x +
			projectile.Velocity.y * projectile.Velocity.y);
		const float return_step = speed * delta_time;
		const float catch_radius = std::max(projectile.Radius * 1.5f, return_step);
		if (distance_sq <= catch_radius * catch_radius)
		{
			projectile.HasReachedOwner = true;
			return owner_position;
		}

		const float distance = std::sqrt(distance_sq);
		projectile.Velocity = {
			to_owner.x / distance * speed,
			to_owner.y / distance * speed,
		};
		return {
			projectile.Position.x + projectile.Velocity.x * delta_time,
			projectile.Position.y + projectile.Velocity.y * delta_time,
		};
	}

	if (projectile.MotionBehavior == ProjectileMotionBehavior::MagicBlade)
	{
		const float launch_speed_sq =
			projectile.MagicBladeLaunchVelocity.x *
				projectile.MagicBladeLaunchVelocity.x +
			projectile.MagicBladeLaunchVelocity.y *
				projectile.MagicBladeLaunchVelocity.y;
		const float launch_speed = std::sqrt(launch_speed_sq);
		if (launch_speed <= 0.0001f)
		{
			return projectile.Position;
		}

		const DirectX::XMFLOAT2 forward = {
			projectile.MagicBladeLaunchVelocity.x / launch_speed,
			projectile.MagicBladeLaunchVelocity.y / launch_speed,
		};
		const float side = projectile.BezierCurveDirection;
		const DirectX::XMFLOAT2 perpendicular = {
			-forward.y * side,
			forward.x * side,
		};
		const DirectX::XMFLOAT2 ready_position = {
			owner_position.x + perpendicular.x * projectile.MagicBladeSideOffset -
				forward.x * 8.0f,
			owner_position.y + perpendicular.y * projectile.MagicBladeSideOffset -
				forward.y * 8.0f,
		};

		if (projectile.Age < projectile.MagicBladeSummonTime)
		{
			const float t = std::clamp(
				projectile.Age / projectile.MagicBladeSummonTime,
				0.0f,
				1.0f);
			const float inverse_t = 1.0f - t;
			const DirectX::XMFLOAT2 control1 = {
				projectile.MagicBladeSummonStart.x - forward.x * 72.0f +
					perpendicular.x * 58.0f,
				projectile.MagicBladeSummonStart.y - forward.y * 72.0f +
					perpendicular.y * 58.0f,
			};
			const DirectX::XMFLOAT2 control2 = {
				ready_position.x - forward.x * 78.0f -
					perpendicular.x * 22.0f,
				ready_position.y - forward.y * 78.0f -
					perpendicular.y * 22.0f,
			};
			const DirectX::XMFLOAT2 next_position = {
				inverse_t * inverse_t * inverse_t *
					projectile.MagicBladeSummonStart.x +
					3.0f * inverse_t * inverse_t * t * control1.x +
					3.0f * inverse_t * t * t * control2.x +
					t * t * t * ready_position.x,
				inverse_t * inverse_t * inverse_t *
					projectile.MagicBladeSummonStart.y +
					3.0f * inverse_t * inverse_t * t * control1.y +
					3.0f * inverse_t * t * t * control2.y +
					t * t * t * ready_position.y,
			};
			if (delta_time > 0.0001f)
			{
				projectile.Velocity = {
					(next_position.x - projectile.Position.x) / delta_time,
					(next_position.y - projectile.Position.y) / delta_time,
				};
				projectile.Rotation = std::atan2(
					projectile.Velocity.x,
					-projectile.Velocity.y);
			}
			return next_position;
		}

		if (projectile.Age < projectile.MagicBladeSummonTime +
			projectile.MagicBladeReadyDelay)
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
			if (GameEnemy::FindNearestAliveForChain(
				owner_position,
				nullptr,
				0,
				1000000.0f,
				aimed_target_id,
				aimed_target))
			{
				const DirectX::XMFLOAT2 to_target = {
					aimed_target.x - projectile.Position.x,
					aimed_target.y - projectile.Position.y,
				};
				const float target_distance_sq =
					to_target.x * to_target.x + to_target.y * to_target.y;
				if (target_distance_sq > 0.0001f)
				{
					const float inverse_target_distance =
						1.0f / std::sqrt(target_distance_sq);
					projectile.Velocity = {
						to_target.x * inverse_target_distance * launch_speed,
						to_target.y * inverse_target_distance * launch_speed,
					};
				}
			}
			projectile.Rotation = std::atan2(
				projectile.Velocity.x,
				-projectile.Velocity.y);
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
		if (GameEnemy::TryGetAlivePosition(
			projectile.BezierTargetID, moving_target))
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
	const float t = std::clamp(
		projectile.BezierElapsed / projectile.BezierDuration, 0.0f, 1.0f);
	const float inverse_t = 1.0f - t;
	const DirectX::XMFLOAT2 next_position = {
		inverse_t * inverse_t * inverse_t * projectile.BezierStart.x +
			3.0f * inverse_t * inverse_t * t * projectile.BezierControl.x +
			3.0f * inverse_t * t * t * projectile.BezierControl2.x +
			t * t * t * projectile.BezierEnd.x,
		inverse_t * inverse_t * inverse_t * projectile.BezierStart.y +
			3.0f * inverse_t * inverse_t * t * projectile.BezierControl.y +
			3.0f * inverse_t * t * t * projectile.BezierControl2.y +
			t * t * t * projectile.BezierEnd.y,
	};
	if (delta_time > 0.0001f)
	{
		projectile.Velocity = {
			(next_position.x - projectile.Position.x) / delta_time,
			(next_position.y - projectile.Position.y) / delta_time,
		};
		projectile.Rotation = std::atan2(
			projectile.Velocity.x, -projectile.Velocity.y);
	}
	if (t >= 1.0f)
	{
		projectile.HasBezierSegment = false;
		projectile.UsesBezierHoming = false;
	}
	return next_position;
}

static DirectX::XMFLOAT2 Projectile_FindLastWalkablePosition(
	const DirectX::XMFLOAT2& start,
	const DirectX::XMFLOAT2& end,
	float radius,
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

static bool Projectile_TryAimBounceAtEnemy(
	cProjectile& projectile,
	const DirectX::XMFLOAT2& impact_position,
	float remaining_time)
{
	constexpr float AIM_RANGE = 900.0f;
	constexpr float MAX_CORRECTION_ANGLE = 1.309f; // 75 degrees.
	constexpr float TWO_PI = 6.28318530718f;

	if (projectile.Layer != CollisionLayer::PlayerBullet)
	{
		return false;
	}

	DirectX::XMFLOAT2 target_position{};
	int target_id = PROJECTILE_INVALID_ID;
	if (!GameEnemy::FindNearestAliveForChain(
		impact_position,
		projectile.HitTargetIDs.data(),
		projectile.TargetHitCount,
		AIM_RANGE,
		target_id,
		target_position))
	{
		return false;
	}

	const float speed_sq =
		projectile.Velocity.x * projectile.Velocity.x +
		projectile.Velocity.y * projectile.Velocity.y;
	const DirectX::XMFLOAT2 to_target = {
		target_position.x - impact_position.x,
		target_position.y - impact_position.y,
	};
	const float target_distance_sq =
		to_target.x * to_target.x + to_target.y * to_target.y;
	if (speed_sq <= 0.0001f || target_distance_sq <= 0.0001f)
	{
		return false;
	}

	const float speed = std::sqrt(speed_sq);
	const float reflected_angle = std::atan2(
		projectile.Velocity.y, projectile.Velocity.x);
	const float target_angle = std::atan2(to_target.y, to_target.x);
	const float angle_delta = std::remainder(
		target_angle - reflected_angle, TWO_PI);
	const float corrected_angle = reflected_angle + std::clamp(
		angle_delta, -MAX_CORRECTION_ANGLE, MAX_CORRECTION_ANGLE);
	const DirectX::XMFLOAT2 corrected_velocity = {
		std::cos(corrected_angle) * speed,
		std::sin(corrected_angle) * speed,
	};

	// Do not let aim correction turn the projectile back into the wall it hit.
	const float probe_time = std::max(
		remaining_time,
		(projectile.Radius * 2.0f + 4.0f) / speed);
	const DirectX::XMFLOAT2 probe_position = {
		impact_position.x + corrected_velocity.x * probe_time,
		impact_position.y + corrected_velocity.y * probe_time,
	};
	if (!ProceduralMap_IsSegmentWalkable(
		impact_position, probe_position, projectile.Radius))
	{
		return false;
	}

	projectile.Velocity = corrected_velocity;
	return true;
}

static bool Projectile_TryBounceOffMap(
	cProjectile& projectile,
	const DirectX::XMFLOAT2& previous_position,
	const DirectX::XMFLOAT2& blocked_position,
	float delta_time)
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
		!ProceduralMap_IsSegmentWalkable(
			previous_position, x_destination, projectile.Radius);
	bool reflect_y = std::abs(movement.y) > 0.0001f &&
		!ProceduralMap_IsSegmentWalkable(
			previous_position, y_destination, projectile.Radius);

	// A diagonal-only collision is a corner hit, so reverse both axes.
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
	const DirectX::XMFLOAT2 impact_position = Projectile_FindLastWalkablePosition(
		previous_position, blocked_position, projectile.Radius, travel_amount);
	if (reflect_x)
	{
		projectile.Velocity.x = -projectile.Velocity.x;
	}
	if (reflect_y)
	{
		projectile.Velocity.y = -projectile.Velocity.y;
	}

	const float remaining_time = delta_time * (1.0f - travel_amount);
	Projectile_TryAimBounceAtEnemy(
		projectile, impact_position, remaining_time);
	projectile.Rotation = std::atan2(
		projectile.Velocity.x, -projectile.Velocity.y);
	const DirectX::XMFLOAT2 reflected_position = {
		impact_position.x + projectile.Velocity.x * remaining_time,
		impact_position.y + projectile.Velocity.y * remaining_time,
	};
	projectile.Position = ProceduralMap_IsSegmentWalkable(
		impact_position, reflected_position, projectile.Radius) ?
		reflected_position : impact_position;
	return true;
}

static float Projectile_GetTrailPixelBlockSize(const cProjectile& projectile)
{
	const float width = projectile.TrailWidth > 0.0f ?
		projectile.TrailWidth : projectile.Width * 0.65f;
	return std::clamp(
		std::round(width * 0.3f * 0.5f) * 2.0f,
		6.0f,
		14.0f);
}

static void Projectile_EmitTrailSample(
	const cProjectile& projectile,
	const DirectX::XMFLOAT2& position)
{
	const float pixel_block_size =
		Projectile_GetTrailPixelBlockSize(projectile);

	cTrailDesc trail_desc{};
	// Begin at the projectile center. The projectile sprite draws over this
	// block, so the visible trail emerges directly from beneath it.
	trail_desc.Position = position;
	trail_desc.Width = pixel_block_size;
	trail_desc.Height = pixel_block_size;
	trail_desc.StartScale = projectile.TrailStartScale;
	trail_desc.EndScale = projectile.TrailEndScale;
	// Screen-aligned blocks keep their edges on the pixel grid. Rotating a
	// square would reintroduce smooth diagonal edges.
	trail_desc.Rotation = 0.0f;
	trail_desc.LifeTime = projectile.TrailLifeTime;
	trail_desc.TextureID = projectile.TrailTextureID != TEXTURE_INVALID_ID ?
		projectile.TrailTextureID :
		projectile.TextureID;
	trail_desc.Color = projectile.TrailColor;
	trail_desc.Pixelated = true;
	trail_desc.PixelGridSize = 2.0f;
	trail_desc.FadeSteps = 5;
	TrailSystem_Emit(trail_desc);
}
static void Projectile_EmitTrail(
	cProjectile& projectile,
	const DirectX::XMFLOAT2& previous_position,
	const DirectX::XMFLOAT2& current_position)
{
	if (!projectile.UsesTrail)
	{
		return;
	}

	const float dx = current_position.x - previous_position.x;
	const float dy = current_position.y - previous_position.y;
	const float segment_length = std::sqrt(dx * dx + dy * dy);
	if (segment_length <= 0.0001f)
	{
		return;
	}

	const DirectX::XMFLOAT2 move_dir = {
		dx / segment_length,
		dy / segment_length,
	};
	const float speed = std::sqrt(
		projectile.Velocity.x * projectile.Velocity.x +
		projectile.Velocity.y * projectile.Velocity.y);
	const float configured_spacing = speed * projectile.TrailEmitInterval;
	const float connected_spacing =
		Projectile_GetTrailPixelBlockSize(projectile) * 0.65f;
	const float emit_spacing = std::max(
		std::min(configured_spacing, connected_spacing), 3.0f);

	float distance_along_segment =
		emit_spacing - projectile.TrailEmitDistance;
	constexpr int MAX_EMITS_PER_UPDATE = 32;
	int emit_count = 0;
	while (distance_along_segment <= segment_length &&
		emit_count < MAX_EMITS_PER_UPDATE)
	{
		const DirectX::XMFLOAT2 sample_position = {
			previous_position.x + move_dir.x * distance_along_segment,
			previous_position.y + move_dir.y * distance_along_segment,
		};
		Projectile_EmitTrailSample(projectile, sample_position);
		distance_along_segment += emit_spacing;
		++emit_count;
	}

	projectile.TrailEmitDistance = std::fmod(
		projectile.TrailEmitDistance + segment_length,
		emit_spacing);
}

void ProjectileSystem_Initialize()
{
	TrailSystem_Initialize();

	for (int i = 0; i < PROJECTILE_MAX; ++i)
	{
		g_Projectiles[i] = cProjectile{};
		g_ActiveIndices[i] = PROJECTILE_INVALID_ID;
		g_FreeProjectiles[i] = PROJECTILE_MAX - 1 - i;
	}

	g_ActiveCount = 0;
	g_FreeCount = PROJECTILE_MAX;
	g_MagicBladeLaunchEvents = 0;
}

void ProjectileSystem_Finalize()
{
	ProjectileSystem_Clear();
	TrailSystem_Finalize();
}

void ProjectileSystem_Clear()
{
	while (g_ActiveCount > 0)
	{
		ProjectileSystem_Deactivate(g_ActiveProjectiles[g_ActiveCount - 1]);
	}

	TrailSystem_Clear();
	g_MagicBladeLaunchEvents = 0;
}

int ProjectileSystem_Fire(const cProjectileDesc& desc)
{
	if (g_FreeCount <= 0)
	{
		return PROJECTILE_INVALID_ID;
	}
	if (desc.AliveLimitGroup != PROJECTILE_INVALID_ID &&
		desc.MaxAliveInGroup > 0)
	{
		int alive_in_group = 0;
		for (int i = 0; i < g_ActiveCount; ++i)
		{
			const cProjectile& active_projectile =
				g_Projectiles[g_ActiveProjectiles[i]];
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

	--g_FreeCount;
	const int projectile_id = g_FreeProjectiles[g_FreeCount];

	cProjectile& projectile = g_Projectiles[projectile_id];
	projectile.IsActive = true;
	projectile.Position = desc.Position;
	projectile.Velocity = desc.Velocity;
	projectile.Radius = Projectile_ClampPositive(desc.Radius, 1.0f);
	projectile.Width = Projectile_ClampPositive(desc.Width, projectile.Radius * 2.0f);
	projectile.Height = Projectile_ClampPositive(desc.Height, projectile.Radius * 2.0f);
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
	projectile.MaxTargetHits = std::clamp(
		desc.MaxTargetHits, 1, PROJECTILE_HIT_HISTORY_MAX);
	projectile.TargetHitCount = 0;
	projectile.HitTargetIDs.fill(PROJECTILE_INVALID_ID);
	projectile.RemainingBounces = std::max(desc.MaxBounces, 0);
	projectile.AliveLimitGroup = desc.AliveLimitGroup;
	projectile.UsesBezierHoming = desc.UsesBezierHoming;
	projectile.BezierCurveStrength = std::max(desc.BezierCurveStrength, 0.0f);
	projectile.BezierCurveDirection = desc.BezierCurveDirection < 0.0f ? -1.0f : 1.0f;
	projectile.BezierSpeed = std::sqrt(
		desc.Velocity.x * desc.Velocity.x + desc.Velocity.y * desc.Velocity.y);
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
	projectile.CanHitTargets =
		desc.MotionBehavior != ProjectileMotionBehavior::MagicBlade;
	if (desc.MotionBehavior == ProjectileMotionBehavior::MagicBlade)
	{
		projectile.Velocity = { 0.0f, 0.0f };
	}
	projectile.UsesTrail = desc.UsesTrail;
	projectile.TrailTextureID = desc.TrailTextureID;
	projectile.TrailEmitInterval = Projectile_ClampPositive(desc.TrailEmitInterval, 0.02f);
	projectile.TrailEmitDistance = 0.0f;
	projectile.TrailWidth = std::max(desc.TrailWidth, 0.0f);
	projectile.TrailLength = std::max(desc.TrailLength, 0.0f);
	projectile.TrailOffset = std::max(desc.TrailOffset, 0.0f);
	projectile.TrailLifeTime = Projectile_ClampPositive(desc.TrailLifeTime, 0.01f);
	projectile.TrailStartScale = Projectile_ClampPositive(desc.TrailStartScale, 1.0f);
	projectile.TrailEndScale = std::max(desc.TrailEndScale, 0.0f);
	projectile.TrailColor = desc.TrailColor;

	g_ActiveIndices[projectile_id] = g_ActiveCount;
	g_ActiveProjectiles[g_ActiveCount] = projectile_id;
	++g_ActiveCount;

	return projectile_id;
}

void ProjectileSystem_Update(
	float delta_time,
	const DirectX::XMFLOAT2& owner_position)
{
	TrailSystem_Update(delta_time);

	for (int active_index = 0; active_index < g_ActiveCount;)
	{
		const int projectile_id = g_ActiveProjectiles[active_index];
		cProjectile& projectile = g_Projectiles[projectile_id];

		projectile.Age += delta_time;
		if (projectile.RepeatHitInterval > 0.0f)
		{
			projectile.RepeatHitTimer += delta_time;
			if (projectile.RepeatHitTimer >= projectile.RepeatHitInterval)
			{
				projectile.RepeatHitTimer = std::fmod(
					projectile.RepeatHitTimer,
					projectile.RepeatHitInterval);
				Projectile_ResetTargetHits(projectile);
			}
		}
		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position =
			Projectile_GetNextPosition(projectile, delta_time, owner_position);
		if (projectile.HasReachedOwner)
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}

		const bool ignores_map =
			projectile.MotionBehavior == ProjectileMotionBehavior::OrbitOwner ||
			(projectile.MotionBehavior == ProjectileMotionBehavior::MagicBlade &&
				!projectile.MagicBladeHasLaunched) ||
			(projectile.MotionBehavior == ProjectileMotionBehavior::Boomerang &&
				projectile.IsReturningToOwner);
		const bool hit_map = !ignores_map && !ProceduralMap_IsSegmentWalkable(
			previous_position, next_position, projectile.Radius);
		if (hit_map &&
			projectile.MotionBehavior == ProjectileMotionBehavior::Boomerang)
		{
			Projectile_BeginBoomerangReturn(projectile);
			projectile.Position = previous_position;
		}
		else if (hit_map && !Projectile_TryBounceOffMap(
			projectile, previous_position, next_position, delta_time))
		{
			ProjectileSystem_Deactivate(projectile_id);
			continue;
		}
		else if (!hit_map)
		{
			projectile.Position = next_position;
		}
		Projectile_EmitTrail(
			projectile, previous_position, projectile.Position);

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

	for (int i = 0; i < g_ActiveCount; ++i)
	{
		const cProjectile& projectile = g_Projectiles[g_ActiveProjectiles[i]];
		if (projectile.TextureID == TEXTURE_INVALID_ID)
		{
			continue;
		}

		auto& projectile_batches =
			projectile.Layer == CollisionLayer::EnemyBullet ? enemy_batches : batches;
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
			SpriteInstanced_DrawOutlinedUnlit(
				texture_id,
				instances.data(),
				static_cast<int>(instances.size()),
				{ 1.0f, 0.035f, 0.015f, 0.96f },
				1.0f);
		}
	}
}

void ProjectileSystem_RegisterColliders()
{
	for (int i = 0; i < g_ActiveCount; ++i)
	{
		const int projectile_id = g_ActiveProjectiles[i];
		const cProjectile& projectile = g_Projectiles[projectile_id];
		CollisionSystem_RegisterCircle(
			projectile_id,
			projectile.Layer,
			projectile.HitMask,
			projectile.Position,
			projectile.Radius,
			projectile.IsActive && projectile.CanHitTargets);
	}
}

void ProjectileSystem_Deactivate(int projectile_id)
{
	if (!Projectile_IsValidID(projectile_id) || !g_Projectiles[projectile_id].IsActive)
	{
		return;
	}

	const int active_index = g_ActiveIndices[projectile_id];
	const int last_active_index = g_ActiveCount - 1;
	const int last_projectile_id = g_ActiveProjectiles[last_active_index];

	g_ActiveProjectiles[active_index] = last_projectile_id;
	g_ActiveIndices[last_projectile_id] = active_index;
	--g_ActiveCount;

	g_Projectiles[projectile_id] = cProjectile{};
	g_ActiveIndices[projectile_id] = PROJECTILE_INVALID_ID;
	g_FreeProjectiles[g_FreeCount] = projectile_id;
	++g_FreeCount;
}

void ProjectileSystem_DeactivateGroup(int alive_limit_group)
{
	if (alive_limit_group == PROJECTILE_INVALID_ID)
	{
		return;
	}
	for (int active_index = 0; active_index < g_ActiveCount;)
	{
		const int projectile_id = g_ActiveProjectiles[active_index];
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
	for (int i = 0; i < g_ActiveCount; ++i)
	{
		const cProjectile& projectile =
			g_Projectiles[g_ActiveProjectiles[i]];
		if (projectile.AliveLimitGroup == alive_limit_group)
		{
			++active_count;
		}
	}
	return active_count;
}

bool ProjectileSystem_IsActive(int projectile_id)
{
	return Projectile_IsValidID(projectile_id) && g_Projectiles[projectile_id].IsActive;
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
	if (!ProjectileSystem_IsActive(projectile_id) || target_id < 0 ||
		!g_Projectiles[projectile_id].CanHitTargets)
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
	return g_ActiveCount;
}

int ProjectileSystem_GetCapacity()
{
	return PROJECTILE_MAX;
}

int ProjectileSystem_ConsumeMagicBladeLaunchEvents()
{
	const int event_count = g_MagicBladeLaunchEvents;
	g_MagicBladeLaunchEvents = 0;
	return event_count;
}
