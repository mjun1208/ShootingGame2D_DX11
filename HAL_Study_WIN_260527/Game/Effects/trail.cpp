#include "trail.h"

#include <array>

#include "indexed_slot_pool.h"
#include "math_utils.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

static std::array<cTrailParticle, TRAIL_MAX> g_TrailParticles{};
static IndexedSlotPool<TRAIL_MAX, TRAIL_INVALID_ID> g_TrailSlots;

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
	}

	g_TrailSlots.Reset();
}

void TrailSystem_Finalize()
{
	TrailSystem_Clear();
}

void TrailSystem_Clear()
{
	while (g_TrailSlots.GetActiveCount() > 0)
	{
		TrailSystem_Deactivate(g_TrailSlots.GetActiveID(g_TrailSlots.GetActiveCount() - 1));
	}
}

int TrailSystem_Emit(const cTrailDesc& desc)
{
	if (!g_TrailSlots.HasFreeSlot() || desc.TextureID == TEXTURE_INVALID_ID)
	{
		return TRAIL_INVALID_ID;
	}

	const int trail_id = g_TrailSlots.Acquire();
	if (trail_id == TRAIL_INVALID_ID)
	{
		return TRAIL_INVALID_ID;
	}

	cTrailParticle& trail = g_TrailParticles[trail_id];
	trail.IsActive = true;
	trail.Position = desc.Position;
	trail.Width = PositiveOr(desc.Width, 1.0f);
	trail.Height = PositiveOr(desc.Height, 1.0f);
	trail.StartScale = PositiveOr(desc.StartScale, 1.0f);
	trail.EndScale = std::max(desc.EndScale, 0.0f);
	trail.Rotation = desc.Rotation;
	trail.Age = 0.0f;
	trail.LifeTime = PositiveOr(desc.LifeTime, 0.01f);
	trail.TextureID = desc.TextureID;
	trail.Color = desc.Color;
	trail.Pixelated = desc.Pixelated;
	trail.PixelGridSize = PositiveOr(desc.PixelGridSize, 1.0f);
	trail.FadeSteps = std::max(desc.FadeSteps, 1);

	return trail_id;
}

void TrailSystem_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	for (int active_index = 0; active_index < g_TrailSlots.GetActiveCount();)
	{
		const int trail_id = g_TrailSlots.GetActiveID(active_index);
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

	for (int i = 0; i < g_TrailSlots.GetActiveCount(); ++i)
	{
		const cTrailParticle& trail = g_TrailParticles[g_TrailSlots.GetActiveID(i)];
		const float t = Saturate(trail.Age / trail.LifeTime);
		const float stepped_t =
		    trail.Pixelated ? std::floor(t * static_cast<float>(trail.FadeSteps)) / static_cast<float>(trail.FadeSteps)
		                    : t;
		float scale = std::lerp(trail.StartScale, trail.EndScale, stepped_t);
		DirectX::XMFLOAT4 color = trail.Color;
		color.w *= Trail_EaseOut(stepped_t);
		DirectX::XMFLOAT2 position = trail.Position;
		if (trail.Pixelated)
		{
			const float grid_size = trail.PixelGridSize;
			position.x = std::round(position.x / grid_size) * grid_size;
			position.y = std::round(position.y / grid_size) * grid_size;
			scale = std::round(scale * static_cast<float>(trail.FadeSteps)) / static_cast<float>(trail.FadeSteps);
		}

		if (color.w <= 0.0f)
		{
			continue;
		}

		batches[trail.TextureID].push_back({
		    position,
		    { trail.Width * scale, trail.Height * scale },
		    trail.Rotation,
		    color,
		});
	}

	for (const auto& [texture_id, instances] : batches)
	{
		if (!instances.empty())
		{
			SpriteInstanced_DrawUnlit(texture_id, instances.data(), static_cast<int>(instances.size()));
		}
	}
}

void TrailSystem_Deactivate(int trail_id)
{
	if (!g_TrailSlots.IsActive(trail_id) || !g_TrailParticles[trail_id].IsActive)
	{
		return;
	}

	g_TrailParticles[trail_id] = cTrailParticle{};
	g_TrailSlots.Release(trail_id);
}

bool TrailSystem_IsActive(int trail_id)
{
	return g_TrailSlots.IsActive(trail_id) && g_TrailParticles[trail_id].IsActive;
}

int TrailSystem_GetActiveCount()
{
	return g_TrailSlots.GetActiveCount();
}

int TrailSystem_GetCapacity()
{
	return g_TrailSlots.GetCapacity();
}
