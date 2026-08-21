#include "projectile.h"

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

static float Projectile_ClampPositive(float value, float fallback)
{
	return value > 0.0f ? value : fallback;
}

static bool Projectile_IsValidID(int projectile_id)
{
	return projectile_id >= 0 && projectile_id < PROJECTILE_MAX;
}

static void Projectile_EmitTrail(cProjectile& projectile, float delta_time)
{
	if (!projectile.UsesTrail)
	{
		return;
	}

	projectile.TrailEmitTimer += delta_time;
	if (projectile.TrailEmitTimer < projectile.TrailEmitInterval)
	{
		return;
	}

	projectile.TrailEmitTimer -= projectile.TrailEmitInterval;

	const float speed_sq =
		projectile.Velocity.x * projectile.Velocity.x +
		projectile.Velocity.y * projectile.Velocity.y;
	if (speed_sq <= 0.0001f)
	{
		return;
	}

	const float speed = std::sqrt(speed_sq);
	const DirectX::XMFLOAT2 move_dir = {
		projectile.Velocity.x / speed,
		projectile.Velocity.y / speed,
	};
	const DirectX::XMFLOAT2 behind_dir = {
		-move_dir.x,
		-move_dir.y,
	};
	const float width = projectile.TrailWidth > 0.0f ? projectile.TrailWidth : projectile.Width * 0.65f;
	const float length = projectile.TrailLength > 0.0f ? projectile.TrailLength : projectile.Height * 1.6f;
	const float offset = projectile.TrailOffset > 0.0f ? projectile.TrailOffset : length * 0.35f;

	cTrailDesc trail_desc{};
	trail_desc.Position = {
		projectile.Position.x + behind_dir.x * offset,
		projectile.Position.y + behind_dir.y * offset,
	};
	trail_desc.Width = width;
	trail_desc.Height = length;
	trail_desc.StartScale = projectile.TrailStartScale;
	trail_desc.EndScale = projectile.TrailEndScale;
	trail_desc.Rotation = std::atan2(move_dir.x, -move_dir.y);
	trail_desc.LifeTime = projectile.TrailLifeTime;
	trail_desc.TextureID = projectile.TrailTextureID != TEXTURE_INVALID_ID ?
		projectile.TrailTextureID :
		projectile.TextureID;
	trail_desc.Color = projectile.TrailColor;
	TrailSystem_Emit(trail_desc);
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
}

int ProjectileSystem_Fire(const cProjectileDesc& desc)
{
	if (g_FreeCount <= 0)
	{
		return PROJECTILE_INVALID_ID;
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
	projectile.UsesTrail = desc.UsesTrail;
	projectile.TrailTextureID = desc.TrailTextureID;
	projectile.TrailEmitInterval = Projectile_ClampPositive(desc.TrailEmitInterval, 0.02f);
	projectile.TrailEmitTimer = 0.0f;
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

void ProjectileSystem_Update(float delta_time)
{
	TrailSystem_Update(delta_time);

	for (int active_index = 0; active_index < g_ActiveCount;)
	{
		const int projectile_id = g_ActiveProjectiles[active_index];
		cProjectile& projectile = g_Projectiles[projectile_id];

		projectile.Age += delta_time;
		Projectile_EmitTrail(projectile, delta_time);
		const DirectX::XMFLOAT2 previous_position = projectile.Position;
		const DirectX::XMFLOAT2 next_position = {
			projectile.Position.x + projectile.Velocity.x * delta_time,
			projectile.Position.y + projectile.Velocity.y * delta_time,
		};
		const bool hit_map = !ProceduralMap_IsSegmentWalkable(
			previous_position, next_position, projectile.Radius);
		projectile.Position = next_position;

		const bool is_life_over = projectile.LifeTime > 0.0f && projectile.Age >= projectile.LifeTime;
		if (is_life_over || hit_map)
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
	for (auto& batch : batches)
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

		batches[projectile.TextureID].push_back({
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
			projectile.IsActive);
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
	if (!ProjectileSystem_IsActive(projectile_id) || target_id < 0)
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
