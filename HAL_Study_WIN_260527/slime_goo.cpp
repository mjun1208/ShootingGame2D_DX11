#include "slime_goo.h"

#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

using namespace DirectX;

namespace
{
	constexpr float PUDDLE_LIFETIME = 15.0f;
	constexpr float PUDDLE_FADE_DURATION = 3.0f;
	constexpr float PUDDLE_BASE_ALPHA = 0.82f;
	constexpr int BASE_DROPLET_COUNT = 18;
	constexpr float DROPLET_GRAVITY = 260.0f;
	constexpr float DROPLET_DRAG_PER_SECOND = 0.045f;
}

cSlimeGoo& cSlimeGoo::GetInstance()
{
	static cSlimeGoo instance;
	return instance;
}

bool cSlimeGoo::Initialize()
{
	if (m_IsInitialized)
	{
		return true;
	}

	m_TextureID = Texture_Load(L"asset/texture/slime_splat.png", false);
	if (m_TextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}

	Clear();
	m_IsInitialized = true;
	return true;
}

void cSlimeGoo::Finalize()
{
	Clear();
	Texture_Release(m_TextureID);
	m_TextureID = TEXTURE_INVALID_ID;
	m_IsInitialized = false;
}

void cSlimeGoo::Clear()
{
	for (Puddle& puddle : m_Puddles)
	{
		puddle = {};
	}
	for (Droplet& droplet : m_Droplets)
	{
		droplet = {};
	}
	for (Pop& pop : m_Pops)
	{
		pop = {};
	}
	m_PuddleCursor = 0;
	m_DropletCursor = 0;
	m_PopCursor = 0;
}

float cSlimeGoo::NextRandom()
{
	m_RandomState ^= m_RandomState << 13;
	m_RandomState ^= m_RandomState >> 17;
	m_RandomState ^= m_RandomState << 5;
	return static_cast<float>(m_RandomState & 0x00FFFFFFu) /
		static_cast<float>(0x00FFFFFFu);
}

void cSlimeGoo::SpawnPuddle(const XMFLOAT2& position, float intensity)
{
	Puddle& puddle = m_Puddles[m_PuddleCursor];
	m_PuddleCursor = (m_PuddleCursor + 1) % PUDDLE_CAPACITY;

	const float intensity_scale = 1.0f + (intensity - 1.0f) * 0.22f;
	const float width = (54.0f + NextRandom() * 24.0f) * intensity_scale;
	puddle.Position = {
		position.x + (NextRandom() * 2.0f - 1.0f) * 11.0f,
		position.y + 13.0f + (NextRandom() * 2.0f - 1.0f) * 7.0f,
	};
	puddle.Size = {
		width,
		width * (0.78f + NextRandom() * 0.22f),
	};
	puddle.Rotation = NextRandom() * XM_2PI;
	puddle.Age = 0.0f;
	puddle.IsActive = true;
}

void cSlimeGoo::Spawn(
	const XMFLOAT2& position,
	const XMFLOAT2& impact_direction,
	float intensity)
{
	if (!m_IsInitialized)
	{
		return;
	}

	intensity = std::clamp(intensity, 0.5f, 2.0f);
	SpawnPuddle(position, intensity);

	XMFLOAT2 direction{};
	const float direction_length_sq =
		impact_direction.x * impact_direction.x +
		impact_direction.y * impact_direction.y;
	if (direction_length_sq > 0.0001f)
	{
		const float inverse_length = 1.0f / std::sqrt(direction_length_sq);
		direction = {
			impact_direction.x * inverse_length,
			impact_direction.y * inverse_length,
		};
	}

	const int droplet_count = std::clamp(
		static_cast<int>(BASE_DROPLET_COUNT * intensity + 0.5f),
		8,
		36);
	for (int i = 0; i < droplet_count; ++i)
	{
		Droplet& droplet = m_Droplets[m_DropletCursor];
		m_DropletCursor = (m_DropletCursor + 1) % DROPLET_CAPACITY;

		const float angle = NextRandom() * XM_2PI;
		const XMFLOAT2 radial = { std::cos(angle), std::sin(angle) };
		const float speed = (115.0f + NextRandom() * 225.0f) *
			(0.8f + intensity * 0.2f);
		const float direction_bias = 35.0f + NextRandom() * 95.0f;
		droplet.Position = {
			position.x + radial.x * NextRandom() * 13.0f,
			position.y - 8.0f + radial.y * NextRandom() * 13.0f,
		};
		droplet.Velocity = {
			radial.x * speed + direction.x * direction_bias,
			radial.y * speed + direction.y * direction_bias - 75.0f,
		};
		droplet.Size = (8.0f + NextRandom() * 15.0f) *
			(0.88f + intensity * 0.12f);
		droplet.Rotation = NextRandom() * XM_2PI;
		droplet.AngularVelocity = (NextRandom() * 2.0f - 1.0f) * 12.0f;
		droplet.Age = 0.0f;
		droplet.Lifetime = 0.38f + NextRandom() * 0.42f;
		droplet.IsActive = true;
	}

	Pop& pop = m_Pops[m_PopCursor];
	m_PopCursor = (m_PopCursor + 1) % POP_CAPACITY;
	pop.Position = { position.x, position.y - 4.0f };
	pop.StartSize = 32.0f * intensity;
	pop.EndSize = 92.0f * intensity;
	pop.Rotation = NextRandom() * XM_2PI;
	pop.Age = 0.0f;
	pop.Lifetime = 0.20f + intensity * 0.035f;
	pop.IsActive = true;
}

void cSlimeGoo::Update(float delta_time)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	for (Puddle& puddle : m_Puddles)
	{
		if (!puddle.IsActive)
		{
			continue;
		}

		puddle.Age += safe_delta_time;
		if (puddle.Age >= PUDDLE_LIFETIME)
		{
			puddle.IsActive = false;
		}
	}

	const float drag = std::pow(DROPLET_DRAG_PER_SECOND, safe_delta_time);
	for (Droplet& droplet : m_Droplets)
	{
		if (!droplet.IsActive)
		{
			continue;
		}

		droplet.Age += safe_delta_time;
		if (droplet.Age >= droplet.Lifetime)
		{
			droplet.IsActive = false;
			continue;
		}

		droplet.Velocity.y += DROPLET_GRAVITY * safe_delta_time;
		droplet.Position.x += droplet.Velocity.x * safe_delta_time;
		droplet.Position.y += droplet.Velocity.y * safe_delta_time;
		droplet.Velocity.x *= drag;
		droplet.Velocity.y *= drag;
		droplet.Rotation += droplet.AngularVelocity * safe_delta_time;
	}

	for (Pop& pop : m_Pops)
	{
		if (!pop.IsActive)
		{
			continue;
		}

		pop.Age += safe_delta_time;
		if (pop.Age >= pop.Lifetime)
		{
			pop.IsActive = false;
		}
	}
}

void cSlimeGoo::DrawGround() const
{
	if (m_TextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	static std::vector<SpriteInstance> instances;
	if (instances.capacity() < PUDDLE_CAPACITY)
	{
		instances.reserve(PUDDLE_CAPACITY);
	}
	instances.clear();

	for (const Puddle& puddle : m_Puddles)
	{
		if (!puddle.IsActive)
		{
			continue;
		}

		const float fade_start = PUDDLE_LIFETIME - PUDDLE_FADE_DURATION;
		const float fade = puddle.Age <= fade_start ? 1.0f :
			std::clamp(
				(PUDDLE_LIFETIME - puddle.Age) / PUDDLE_FADE_DURATION,
				0.0f,
				1.0f);
		instances.push_back({
			puddle.Position,
			puddle.Size,
			puddle.Rotation,
			{ 1.0f, 1.0f, 1.0f, PUDDLE_BASE_ALPHA * fade },
		});
	}

	if (!instances.empty())
	{
		SpriteInstanced_Draw(
			m_TextureID,
			instances.data(),
			static_cast<int>(instances.size()));
	}
}

void cSlimeGoo::DrawBurst() const
{
	if (m_TextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	static std::vector<SpriteInstance> instances;
	constexpr std::size_t INSTANCE_CAPACITY = DROPLET_CAPACITY + POP_CAPACITY;
	if (instances.capacity() < INSTANCE_CAPACITY)
	{
		instances.reserve(INSTANCE_CAPACITY);
	}
	instances.clear();

	for (const Pop& pop : m_Pops)
	{
		if (!pop.IsActive || pop.Lifetime <= 0.0f)
		{
			continue;
		}

		const float progress = std::clamp(pop.Age / pop.Lifetime, 0.0f, 1.0f);
		const float ease_out = 1.0f - (1.0f - progress) * (1.0f - progress);
		const float size = pop.StartSize + (pop.EndSize - pop.StartSize) * ease_out;
		const float alpha = (1.0f - progress) * (1.0f - progress) * 0.72f;
		instances.push_back({
			pop.Position,
			{ size, size },
			pop.Rotation,
			{ 1.0f, 1.0f, 1.0f, alpha },
		});
	}

	for (const Droplet& droplet : m_Droplets)
	{
		if (!droplet.IsActive || droplet.Lifetime <= 0.0f)
		{
			continue;
		}

		const float progress = std::clamp(
			droplet.Age / droplet.Lifetime, 0.0f, 1.0f);
		const float size = droplet.Size * (1.0f - progress * 0.42f);
		const float alpha = (1.0f - progress) * 0.95f;
		instances.push_back({
			droplet.Position,
			{ size, size },
			droplet.Rotation,
			{ 1.0f, 1.0f, 1.0f, alpha },
		});
	}

	if (!instances.empty())
	{
		SpriteInstanced_Draw(
			m_TextureID,
			instances.data(),
			static_cast<int>(instances.size()));
	}
}
