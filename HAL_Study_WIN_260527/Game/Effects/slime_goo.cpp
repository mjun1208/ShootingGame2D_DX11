#include "slime_goo.h"

#include "math_utils.h"
#include "random_utils.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{

	namespace SlimeGooTuning::Puddle
	{
		constexpr float Lifetime = 15.0f;
	} // namespace SlimeGooTuning::Puddle

} // namespace

using namespace DirectX;

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

void cSlimeGoo::SpawnPuddle(const XMFLOAT2& position, float intensity)
{
	Puddle& puddle = m_Puddles[m_PuddleCursor];
	m_PuddleCursor = (m_PuddleCursor + 1) % PUDDLE_CAPACITY;

	const float intensity_scale = 1.0f + (intensity - 1.0f) * 0.22f;
	const float width = (54.0f + Random01() * 24.0f) * intensity_scale;
	puddle.Position = {
		position.x + RandomSigned() * 11.0f,
		position.y + 13.0f + RandomSigned() * 7.0f,
	};
	puddle.Size = {
		width,
		width * (0.78f + Random01() * 0.22f),
	};
	puddle.Rotation = Random01() * XM_2PI;
	puddle.Age = 0.0f;
	puddle.IsActive = true;
}

void cSlimeGoo::Spawn(const XMFLOAT2& position, const XMFLOAT2& impact_direction, float intensity)
{
	static constexpr int BaseDropletCount = 18;

	if (!m_IsInitialized)
	{
		return;
	}

	intensity = std::clamp(intensity, 0.5f, 2.0f);
	SpawnPuddle(position, intensity);

	const XMFLOAT2 direction = NormalizeOr(impact_direction, { 0.0f, 0.0f });

	const int droplet_count = std::clamp(static_cast<int>(BaseDropletCount * intensity + 0.5f), 8, 36);
	for (int i = 0; i < droplet_count; ++i)
	{
		Droplet& droplet = m_Droplets[m_DropletCursor];
		m_DropletCursor = (m_DropletCursor + 1) % DROPLET_CAPACITY;

		const float angle = Random01() * XM_2PI;
		const XMFLOAT2 radial = { std::cos(angle), std::sin(angle) };
		const float speed = (115.0f + Random01() * 225.0f) * (0.8f + intensity * 0.2f);
		const float direction_bias = 35.0f + Random01() * 95.0f;
		droplet.Position = {
			position.x + radial.x * Random01() * 13.0f,
			position.y - 8.0f + radial.y * Random01() * 13.0f,
		};
		droplet.Velocity = {
			radial.x * speed + direction.x * direction_bias,
			radial.y * speed + direction.y * direction_bias - 75.0f,
		};
		droplet.Size = (8.0f + Random01() * 15.0f) * (0.88f + intensity * 0.12f);
		droplet.Rotation = Random01() * XM_2PI;
		droplet.AngularVelocity = RandomSigned() * 12.0f;
		droplet.Age = 0.0f;
		droplet.Lifetime = 0.38f + Random01() * 0.42f;
		droplet.IsActive = true;
	}

	Pop& pop = m_Pops[m_PopCursor];
	m_PopCursor = (m_PopCursor + 1) % POP_CAPACITY;
	pop.Position = { position.x, position.y - 4.0f };
	pop.StartSize = 32.0f * intensity;
	pop.EndSize = 92.0f * intensity;
	pop.Rotation = Random01() * XM_2PI;
	pop.Age = 0.0f;
	pop.Lifetime = 0.20f + intensity * 0.035f;
	pop.IsActive = true;
}

void cSlimeGoo::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	static constexpr float DragPerSecond = 0.045f;
	static constexpr float Gravity = 260.0f;

	const float safe_delta_time = delta_time;
	for (Puddle& puddle : m_Puddles)
	{
		if (!puddle.IsActive)
		{
			continue;
		}

		puddle.Age += safe_delta_time;
		if (puddle.Age >= SlimeGooTuning::Puddle::Lifetime)
		{
			puddle.IsActive = false;
		}
	}

	const float drag = std::pow(DragPerSecond, safe_delta_time);
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

		droplet.Velocity.y += Gravity * safe_delta_time;
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
	static constexpr float BaseAlpha = 0.82f;
	static constexpr float FadeDuration = 3.0f;

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

		const float fade_start = SlimeGooTuning::Puddle::Lifetime - FadeDuration;
		const float fade =
		    puddle.Age <= fade_start ? 1.0f : Saturate((SlimeGooTuning::Puddle::Lifetime - puddle.Age) / FadeDuration);
		instances.push_back({
		    puddle.Position,
		    puddle.Size,
		    puddle.Rotation,
		    { 1.0f, 1.0f, 1.0f, BaseAlpha * fade },
		});
	}

	if (!instances.empty())
	{
		SpriteInstanced_Draw(m_TextureID, instances.data(), static_cast<int>(instances.size()));
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

		const float progress = Saturate(pop.Age / pop.Lifetime);
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

		const float progress = Saturate(droplet.Age / droplet.Lifetime);
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
		SpriteInstanced_DrawUnlit(m_TextureID, instances.data(), static_cast<int>(instances.size()));
	}
}
