#include "trail.h"

#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

static cTrailParticle g_TrailParticles[TRAIL_MAX];
static int g_ActiveTrails[TRAIL_MAX];
static int g_ActiveIndices[TRAIL_MAX];
static int g_FreeTrails[TRAIL_MAX];
static int g_ActiveCount = 0;
static int g_FreeCount = 0;

static float Trail_ClampPositive(float value, float fallback)
{
	return value > 0.0f ? value : fallback;
}

static bool Trail_IsValidID(int trail_id)
{
	return trail_id >= 0 && trail_id < TRAIL_MAX;
}

static float Trail_Lerp(float a, float b, float t)
{
	return a + (b - a) * t;
}

static float Trail_EaseOut(float t)
{
	const float inv_t = 1.0f - t;
	return inv_t * inv_t;
}

void TrailSystem_Initialize()
{
	for (int i = 0; i < TRAIL_MAX; ++i)
	{
		g_TrailParticles[i] = cTrailParticle{};
		g_ActiveIndices[i] = TRAIL_INVALID_ID;
		g_FreeTrails[i] = TRAIL_MAX - 1 - i;
	}

	g_ActiveCount = 0;
	g_FreeCount = TRAIL_MAX;
}

void TrailSystem_Finalize()
{
	TrailSystem_Clear();
}

void TrailSystem_Clear()
{
	while (g_ActiveCount > 0)
	{
		TrailSystem_Deactivate(g_ActiveTrails[g_ActiveCount - 1]);
	}
}

int TrailSystem_Emit(const cTrailDesc& desc)
{
	if (g_FreeCount <= 0 || desc.TextureID == TEXTURE_INVALID_ID)
	{
		return TRAIL_INVALID_ID;
	}

	--g_FreeCount;
	const int trail_id = g_FreeTrails[g_FreeCount];

	cTrailParticle& trail = g_TrailParticles[trail_id];
	trail.IsActive = true;
	trail.Position = desc.Position;
	trail.Width = Trail_ClampPositive(desc.Width, 1.0f);
	trail.Height = Trail_ClampPositive(desc.Height, 1.0f);
	trail.StartScale = Trail_ClampPositive(desc.StartScale, 1.0f);
	trail.EndScale = std::max(desc.EndScale, 0.0f);
	trail.Rotation = desc.Rotation;
	trail.Age = 0.0f;
	trail.LifeTime = Trail_ClampPositive(desc.LifeTime, 0.01f);
	trail.TextureID = desc.TextureID;
	trail.Color = desc.Color;

	g_ActiveIndices[trail_id] = g_ActiveCount;
	g_ActiveTrails[g_ActiveCount] = trail_id;
	++g_ActiveCount;

	return trail_id;
}

void TrailSystem_Update(float delta_time)
{
	for (int active_index = 0; active_index < g_ActiveCount;)
	{
		const int trail_id = g_ActiveTrails[active_index];
		cTrailParticle& trail = g_TrailParticles[trail_id];

		trail.Age += delta_time;
		if (trail.Age >= trail.LifeTime)
		{
			TrailSystem_Deactivate(trail_id);
			continue;
		}

		++active_index;
	}
}

void TrailSystem_Draw()
{
	static std::unordered_map<int, std::vector<SpriteInstance>> batches;
	for (auto& batch : batches)
	{
		batch.second.clear();
	}

	for (int i = 0; i < g_ActiveCount; ++i)
	{
		const cTrailParticle& trail = g_TrailParticles[g_ActiveTrails[i]];
		const float t = std::clamp(trail.Age / trail.LifeTime, 0.0f, 1.0f);
		const float scale = Trail_Lerp(trail.StartScale, trail.EndScale, t);
		DirectX::XMFLOAT4 color = trail.Color;
		color.w *= Trail_EaseOut(t);

		if (color.w <= 0.0f)
		{
			continue;
		}

		batches[trail.TextureID].push_back({
			trail.Position,
			{ trail.Width * scale, trail.Height * scale },
			trail.Rotation,
			color,
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

void TrailSystem_Deactivate(int trail_id)
{
	if (!Trail_IsValidID(trail_id) || !g_TrailParticles[trail_id].IsActive)
	{
		return;
	}

	const int active_index = g_ActiveIndices[trail_id];
	const int last_active_index = g_ActiveCount - 1;
	const int last_trail_id = g_ActiveTrails[last_active_index];

	g_ActiveTrails[active_index] = last_trail_id;
	g_ActiveIndices[last_trail_id] = active_index;
	--g_ActiveCount;

	g_TrailParticles[trail_id] = cTrailParticle{};
	g_ActiveIndices[trail_id] = TRAIL_INVALID_ID;
	g_FreeTrails[g_FreeCount] = trail_id;
	++g_FreeCount;
}

bool TrailSystem_IsActive(int trail_id)
{
	return Trail_IsValidID(trail_id) && g_TrailParticles[trail_id].IsActive;
}

int TrailSystem_GetActiveCount()
{
	return g_ActiveCount;
}

int TrailSystem_GetCapacity()
{
	return TRAIL_MAX;
}
