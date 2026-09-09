#include "blood.h"

#include "random_utils.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <vector>

namespace
{
	namespace BloodTuning
	{
		constexpr int PARTICLE_CAPACITY = 192;
	} // namespace BloodTuning

} // namespace

using namespace DirectX;

namespace
{
	struct BloodParticle
	{
		XMFLOAT2 Position{};
		XMFLOAT2 Velocity{};
		float GroundY{ 0.0f };
		float Size{ 0.0f };
		float Rotation{ 0.0f };
		float AngularVelocity{ 0.0f };
		bool IsActive{ false };
		bool IsSettled{ false };
	};

	std::array<BloodParticle, BloodTuning::PARTICLE_CAPACITY> g_Particles{};
	int g_ParticleCursor = 0;
	int g_TextureID = TEXTURE_INVALID_ID;

} // namespace

namespace Blood
{
	bool Initialize()
	{
		Finalize();
		g_TextureID = Texture_Load(L"asset/texture/white_square.png", false);
		Clear();
		return g_TextureID != TEXTURE_INVALID_ID;
	}

	void Finalize()
	{
		Clear();
		Texture_Release(g_TextureID);
		g_TextureID = TEXTURE_INVALID_ID;
	}

	void Clear()
	{
		for (BloodParticle& particle : g_Particles)
		{
			particle = {};
		}
		g_ParticleCursor = 0;
	}

	void Spawn(const XMFLOAT2& position)
	{
		static constexpr int PARTICLE_SPAWN_COUNT = 14;
		for (int i = 0; i < PARTICLE_SPAWN_COUNT; ++i)
		{
			BloodParticle& particle = g_Particles[g_ParticleCursor];
			g_ParticleCursor = (g_ParticleCursor + 1) % BloodTuning::PARTICLE_CAPACITY;
			particle.Position = {
				position.x + RandomSigned() * 25.0f,
				position.y - 38.0f + Random01() * 46.0f,
			};
			particle.Velocity = {
				RandomSigned() * 105.0f,
				20.0f + Random01() * 95.0f,
			};
			particle.GroundY = position.y + 42.0f + Random01() * 30.0f;
			particle.Size = 6.0f + Random01() * 8.0f;
			particle.Rotation = Random01() * XM_2PI;
			particle.AngularVelocity = RandomSigned() * 5.5f;
			particle.IsActive = true;
			particle.IsSettled = false;
		}
	}

	void Update(float delta_time)
	{
		delta_time = std::max(delta_time, 0.0f);
		static constexpr float PARTICLE_GRAVITY = 420.0f;
		const float safe_delta_time = delta_time;
		for (BloodParticle& particle : g_Particles)
		{
			if (!particle.IsActive || particle.IsSettled)
			{
				continue;
			}
			particle.Velocity.y += PARTICLE_GRAVITY * safe_delta_time;
			particle.Position.x += particle.Velocity.x * safe_delta_time;
			particle.Position.y += particle.Velocity.y * safe_delta_time;
			particle.Rotation += particle.AngularVelocity * safe_delta_time;
			if (particle.Position.y >= particle.GroundY)
			{
				particle.Position.y = particle.GroundY;
				particle.Velocity = { 0.0f, 0.0f };
				particle.AngularVelocity = 0.0f;
				particle.IsSettled = true;
			}
		}
	}

	void Draw()
	{
		static constexpr DirectX::XMFLOAT4 BLOOD_COLOR{ 1.0f, 0.15f, 0.15f, 1.0f };
		if (g_TextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		static std::vector<SpriteInstance> instances;
		if (instances.capacity() < BloodTuning::PARTICLE_CAPACITY)
		{
			instances.reserve(BloodTuning::PARTICLE_CAPACITY);
		}
		instances.clear();
		for (const BloodParticle& particle : g_Particles)
		{
			if (!particle.IsActive)
			{
				continue;
			}
			instances.push_back({
			    particle.Position,
			    { particle.Size, particle.Size },
			    particle.Rotation,
			    BLOOD_COLOR,
			});
		}
		if (!instances.empty())
		{
			SpriteInstanced_Draw(g_TextureID, instances.data(), static_cast<int>(instances.size()));
		}
	}
} // namespace Blood
