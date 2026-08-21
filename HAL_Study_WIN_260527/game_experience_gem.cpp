#include "game_experience_gem.h"

#include "game_player.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
	constexpr int EXPERIENCE_GEM_MAX = 1024;
	constexpr int EXPERIENCE_GEM_GRID_BUCKET_COUNT = 521;
	constexpr int EXPERIENCE_GEM_INVALID_INDEX = -1;
	constexpr float EXPERIENCE_GEM_GRID_CELL_SIZE = 192.0f;
	constexpr float EXPERIENCE_GEM_ATTRACTION_RADIUS = 280.0f;
	constexpr float EXPERIENCE_GEM_ATTRACTION_RADIUS_SQ =
		EXPERIENCE_GEM_ATTRACTION_RADIUS * EXPERIENCE_GEM_ATTRACTION_RADIUS;
	constexpr float EXPERIENCE_GEM_ABSORB_RADIUS = 36.0f;
	constexpr float EXPERIENCE_GEM_ABSORB_RADIUS_SQ =
		EXPERIENCE_GEM_ABSORB_RADIUS * EXPERIENCE_GEM_ABSORB_RADIUS;
	constexpr float EXPERIENCE_GEM_START_SPEED = 105.0f;
	constexpr float EXPERIENCE_GEM_SCATTER_DECELERATION = 360.0f;
	constexpr float EXPERIENCE_GEM_MAGNET_START_SPEED = 260.0f;
	constexpr float EXPERIENCE_GEM_MAGNET_ACCELERATION = 1500.0f;
	constexpr float EXPERIENCE_GEM_MAGNET_MAX_SPEED = 1050.0f;
	constexpr float EXPERIENCE_GEM_DRAW_SIZE = 24.0f;
	constexpr float EXPERIENCE_GEM_GLOW_SIZE = 40.0f;
	constexpr float EXPERIENCE_GEM_BOB_AMOUNT = 2.5f;

	struct ExperienceGem
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float MagnetSpeed{ 0.0f };
		float Age{ 0.0f };
		int Experience{ 0 };
		bool Active{ false };
		bool Magnetized{ false };
	};

	struct ExperienceGemGrid
	{
		int BucketHeads[EXPERIENCE_GEM_GRID_BUCKET_COUNT]{};
		int NextGem[EXPERIENCE_GEM_MAX]{};
		int CellX[EXPERIENCE_GEM_MAX]{};
		int CellY[EXPERIENCE_GEM_MAX]{};
	};

	ExperienceGem g_ExperienceGems[EXPERIENCE_GEM_MAX];
	ExperienceGemGrid g_ExperienceGemGrid;
	int g_ExperienceGemTextureID = TEXTURE_INVALID_ID;
	std::uint32_t g_SpawnSerial = 0;

	std::uint32_t Hash32(std::uint32_t value)
	{
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		value ^= value >> 16;
		return value;
	}

	int WorldToCell(float value)
	{
		return static_cast<int>(std::floor(value / EXPERIENCE_GEM_GRID_CELL_SIZE));
	}

	int HashCell(int cell_x, int cell_y)
	{
		const std::uint32_t hash =
			static_cast<std::uint32_t>(cell_x) * 0x8da6b343u ^
			static_cast<std::uint32_t>(cell_y) * 0xd8163841u;
		return static_cast<int>(hash % EXPERIENCE_GEM_GRID_BUCKET_COUNT);
	}

	void BuildGrid()
	{
		std::fill_n(
			g_ExperienceGemGrid.BucketHeads,
			EXPERIENCE_GEM_GRID_BUCKET_COUNT,
			EXPERIENCE_GEM_INVALID_INDEX);
		std::fill_n(
			g_ExperienceGemGrid.NextGem,
			EXPERIENCE_GEM_MAX,
			EXPERIENCE_GEM_INVALID_INDEX);

		for (int gem_id = 0; gem_id < EXPERIENCE_GEM_MAX; ++gem_id)
		{
			const ExperienceGem& gem = g_ExperienceGems[gem_id];
			if (!gem.Active || gem.Magnetized)
			{
				continue;
			}

			const int cell_x = WorldToCell(gem.Position.x);
			const int cell_y = WorldToCell(gem.Position.y);
			const int bucket = HashCell(cell_x, cell_y);
			g_ExperienceGemGrid.CellX[gem_id] = cell_x;
			g_ExperienceGemGrid.CellY[gem_id] = cell_y;
			g_ExperienceGemGrid.NextGem[gem_id] =
				g_ExperienceGemGrid.BucketHeads[bucket];
			g_ExperienceGemGrid.BucketHeads[bucket] = gem_id;
		}
	}

	void AttractNearbyGems(const DirectX::XMFLOAT2& player_position)
	{
		const int min_cell_x = WorldToCell(
			player_position.x - EXPERIENCE_GEM_ATTRACTION_RADIUS);
		const int max_cell_x = WorldToCell(
			player_position.x + EXPERIENCE_GEM_ATTRACTION_RADIUS);
		const int min_cell_y = WorldToCell(
			player_position.y - EXPERIENCE_GEM_ATTRACTION_RADIUS);
		const int max_cell_y = WorldToCell(
			player_position.y + EXPERIENCE_GEM_ATTRACTION_RADIUS);

		for (int cell_y = min_cell_y; cell_y <= max_cell_y; ++cell_y)
		{
			for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x)
			{
				const int bucket = HashCell(cell_x, cell_y);
				for (int gem_id = g_ExperienceGemGrid.BucketHeads[bucket];
					gem_id != EXPERIENCE_GEM_INVALID_INDEX;
					gem_id = g_ExperienceGemGrid.NextGem[gem_id])
				{
					if (g_ExperienceGemGrid.CellX[gem_id] != cell_x ||
						g_ExperienceGemGrid.CellY[gem_id] != cell_y)
					{
						continue;
					}

					ExperienceGem& gem = g_ExperienceGems[gem_id];
					const float dx = player_position.x - gem.Position.x;
					const float dy = player_position.y - gem.Position.y;
					if (dx * dx + dy * dy <= EXPERIENCE_GEM_ATTRACTION_RADIUS_SQ)
					{
						gem.Magnetized = true;
						gem.MagnetSpeed = EXPERIENCE_GEM_MAGNET_START_SPEED;
						gem.Velocity = { 0.0f, 0.0f };
					}
				}
			}
		}
	}
}

namespace GameExperienceGem
{
void Initialize()
{
	g_ExperienceGemTextureID = Texture_Load(
		L"asset/texture/experience_gem.png", false);
	Clear();
}

void Finalize()
{
	Clear();
	Texture_Release(g_ExperienceGemTextureID);
	g_ExperienceGemTextureID = TEXTURE_INVALID_ID;
}

void Clear()
{
	for (ExperienceGem& gem : g_ExperienceGems)
	{
		gem = ExperienceGem{};
	}
	g_SpawnSerial = 0;
	BuildGrid();
}

void Spawn(const DirectX::XMFLOAT2& world_position, int experience)
{
	if (experience <= 0)
	{
		return;
	}

	int free_id = EXPERIENCE_GEM_INVALID_INDEX;
	for (int gem_id = 0; gem_id < EXPERIENCE_GEM_MAX; ++gem_id)
	{
		if (!g_ExperienceGems[gem_id].Active)
		{
			free_id = gem_id;
			break;
		}
	}
	if (free_id == EXPERIENCE_GEM_INVALID_INDEX)
	{
		return;
	}

	const std::uint32_t scatter_hash = Hash32(++g_SpawnSerial);
	const float angle = static_cast<float>(scatter_hash & 0xffffu) /
		65535.0f * DirectX::XM_2PI;
	const float speed_scale = 0.72f +
		static_cast<float>((scatter_hash >> 16) & 0xffu) / 255.0f * 0.56f;
	ExperienceGem& gem = g_ExperienceGems[free_id];
	gem.Position = world_position;
	gem.Velocity = {
		std::cos(angle) * EXPERIENCE_GEM_START_SPEED * speed_scale,
		std::sin(angle) * EXPERIENCE_GEM_START_SPEED * speed_scale,
	};
	gem.Experience = experience;
	gem.Active = true;
}

void Update(float delta_time, const DirectX::XMFLOAT2& player_position)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	for (ExperienceGem& gem : g_ExperienceGems)
	{
		if (!gem.Active || gem.Magnetized)
		{
			continue;
		}

		gem.Age += safe_delta_time;
		gem.Position.x += gem.Velocity.x * safe_delta_time;
		gem.Position.y += gem.Velocity.y * safe_delta_time;
		const float speed_sq =
			gem.Velocity.x * gem.Velocity.x + gem.Velocity.y * gem.Velocity.y;
		if (speed_sq > 0.0001f)
		{
			const float speed = std::sqrt(speed_sq);
			const float next_speed = std::max(
				0.0f, speed - EXPERIENCE_GEM_SCATTER_DECELERATION * safe_delta_time);
			const float speed_ratio = next_speed / speed;
			gem.Velocity.x *= speed_ratio;
			gem.Velocity.y *= speed_ratio;
		}
	}

	BuildGrid();
	AttractNearbyGems(player_position);

	for (ExperienceGem& gem : g_ExperienceGems)
	{
		if (!gem.Active || !gem.Magnetized)
		{
			continue;
		}

		gem.Age += safe_delta_time;
		const float dx = player_position.x - gem.Position.x;
		const float dy = player_position.y - gem.Position.y;
		const float distance_sq = dx * dx + dy * dy;
		if (distance_sq <= EXPERIENCE_GEM_ABSORB_RADIUS_SQ)
		{
			GamePlayer::AddExperience(gem.Experience);
			gem = ExperienceGem{};
			continue;
		}

		const float distance = std::sqrt(distance_sq);
		gem.MagnetSpeed = std::min(
			EXPERIENCE_GEM_MAGNET_MAX_SPEED,
			gem.MagnetSpeed + EXPERIENCE_GEM_MAGNET_ACCELERATION * safe_delta_time);
		const float move_distance = std::min(
			gem.MagnetSpeed * safe_delta_time, distance);
		gem.Position.x += dx / distance * move_distance;
		gem.Position.y += dy / distance * move_distance;
	}
}

void Draw()
{
	if (g_ExperienceGemTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	static std::vector<SpriteInstance> gem_instances;
	static std::vector<SpriteInstance> glow_instances;
	if (gem_instances.capacity() < EXPERIENCE_GEM_MAX)
	{
		gem_instances.reserve(EXPERIENCE_GEM_MAX);
		glow_instances.reserve(EXPERIENCE_GEM_MAX);
	}
	gem_instances.clear();
	glow_instances.clear();

	for (const ExperienceGem& gem : g_ExperienceGems)
	{
		if (!gem.Active)
		{
			continue;
		}

		const float pulse_phase = std::sin(gem.Age * 7.0f);
		const float pulse = 1.0f + pulse_phase * 0.10f;
		const DirectX::XMFLOAT2 draw_position = {
			gem.Position.x,
			gem.Position.y + std::sin(gem.Age * 4.0f) * EXPERIENCE_GEM_BOB_AMOUNT,
		};
		glow_instances.push_back({
			draw_position,
			{ EXPERIENCE_GEM_GLOW_SIZE * pulse, EXPERIENCE_GEM_GLOW_SIZE * pulse },
			0.0f,
			{ 0.16f, 0.82f, 1.0f, 0.46f + pulse_phase * 0.08f },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			1.0f,
		});
		gem_instances.push_back({
			draw_position,
			{ EXPERIENCE_GEM_DRAW_SIZE * pulse, EXPERIENCE_GEM_DRAW_SIZE * pulse },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
		});
	}

	if (!glow_instances.empty())
	{
		SpriteInstanced_DrawAdditive(
			g_ExperienceGemTextureID,
			glow_instances.data(),
			static_cast<int>(glow_instances.size()));
		// A second additive pass makes the gem itself emissive without consuming
		// one of the renderer's limited point-light slots per pickup.
		SpriteInstanced_DrawAdditive(
			g_ExperienceGemTextureID,
			gem_instances.data(),
			static_cast<int>(gem_instances.size()));
		SpriteInstanced_Draw(
			g_ExperienceGemTextureID,
			gem_instances.data(),
			static_cast<int>(gem_instances.size()));
	}
}
}
