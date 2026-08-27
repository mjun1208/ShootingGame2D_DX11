#include "game_healing_item.h"

#include "game_player.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace
{
	constexpr int HEALING_ITEM_MAX = 128;
	constexpr float HEALING_ITEM_DROP_CHANCE = 0.12f;
	constexpr float HEALING_ITEM_HEAL_AMOUNT = 25.0f;
	constexpr float HEALING_ITEM_ATTRACTION_RADIUS = 230.0f;
	constexpr float HEALING_ITEM_ATTRACTION_RADIUS_SQ =
		HEALING_ITEM_ATTRACTION_RADIUS * HEALING_ITEM_ATTRACTION_RADIUS;
	constexpr float HEALING_ITEM_ABSORB_RADIUS = 38.0f;
	constexpr float HEALING_ITEM_ABSORB_RADIUS_SQ =
		HEALING_ITEM_ABSORB_RADIUS * HEALING_ITEM_ABSORB_RADIUS;
	constexpr float HEALING_ITEM_START_SPEED = 92.0f;
	constexpr float HEALING_ITEM_SCATTER_DECELERATION = 320.0f;
	constexpr float HEALING_ITEM_MAGNET_START_SPEED = 240.0f;
	constexpr float HEALING_ITEM_MAGNET_ACCELERATION = 1350.0f;
	constexpr float HEALING_ITEM_MAGNET_MAX_SPEED = 960.0f;
	constexpr float HEALING_ITEM_DRAW_SIZE = 38.0f;
	constexpr float HEALING_ITEM_GLOW_SIZE = 54.0f;
	constexpr float HEALING_ITEM_BOB_AMOUNT = 3.0f;
	constexpr std::uint32_t HEALING_ITEM_RANDOM_SEED = 0x48EA17A5u;

	struct HealingItem
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Velocity{};
		float MagnetSpeed{ 0.0f };
		float Age{ 0.0f };
		int RoomIndex{ -1 };
		bool Active{ false };
		bool Magnetized{ false };
	};

	std::array<HealingItem, HEALING_ITEM_MAX> g_HealingItems{};
	int g_HealingItemTextureID = TEXTURE_INVALID_ID;
	std::uint32_t g_DropRandomState = HEALING_ITEM_RANDOM_SEED;
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

	bool RollDropChance()
	{
		g_DropRandomState = Hash32(g_DropRandomState + 0x9e3779b9u);
		const double normalized = static_cast<double>(g_DropRandomState) /
			static_cast<double>(std::numeric_limits<std::uint32_t>::max());
		return normalized < static_cast<double>(HEALING_ITEM_DROP_CHANCE);
	}
}

namespace GameHealingItem
{
void Initialize()
{
	g_HealingItemTextureID = Texture_Load(
		L"asset/texture/healing_potion.png", false);
	Clear();
}

void Finalize()
{
	Clear();
	Texture_Release(g_HealingItemTextureID);
	g_HealingItemTextureID = TEXTURE_INVALID_ID;
}

void Clear()
{
	g_HealingItems.fill(HealingItem{});
	g_DropRandomState = HEALING_ITEM_RANDOM_SEED;
	g_SpawnSerial = 0;
}

void TrySpawn(
	const DirectX::XMFLOAT2& world_position,
	int room_index)
{
	if (!RollDropChance())
	{
		return;
	}

	auto free_item = std::find_if(
		g_HealingItems.begin(),
		g_HealingItems.end(),
		[](const HealingItem& item)
		{
			return !item.Active;
		});
	if (free_item == g_HealingItems.end())
	{
		return;
	}

	const std::uint32_t scatter_hash = Hash32(++g_SpawnSerial);
	const float angle = static_cast<float>(scatter_hash & 0xffffu) /
		65535.0f * DirectX::XM_2PI;
	const float speed_scale = 0.75f +
		static_cast<float>((scatter_hash >> 16) & 0xffu) / 255.0f * 0.50f;
	free_item->Position = world_position;
	free_item->Velocity = {
		std::cos(angle) * HEALING_ITEM_START_SPEED * speed_scale,
		std::sin(angle) * HEALING_ITEM_START_SPEED * speed_scale,
	};
	free_item->RoomIndex = room_index;
	free_item->Active = true;
}

void Update(float delta_time, const DirectX::XMFLOAT2& player_position)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	const bool player_can_heal =
		GamePlayer::GetHitPoint() > 0.0f &&
		GamePlayer::GetHitPoint() < GamePlayer::GetMaxHitPoint();

	for (HealingItem& item : g_HealingItems)
	{
		if (!item.Active)
		{
			continue;
		}

		item.Age += safe_delta_time;
		if (!item.Magnetized)
		{
			item.Position.x += item.Velocity.x * safe_delta_time;
			item.Position.y += item.Velocity.y * safe_delta_time;
			const float speed_sq =
				item.Velocity.x * item.Velocity.x +
				item.Velocity.y * item.Velocity.y;
			if (speed_sq > 0.0001f)
			{
				const float speed = std::sqrt(speed_sq);
				const float next_speed = std::max(
					0.0f,
					speed - HEALING_ITEM_SCATTER_DECELERATION * safe_delta_time);
				const float speed_ratio = next_speed / speed;
				item.Velocity.x *= speed_ratio;
				item.Velocity.y *= speed_ratio;
			}

			const float dx = player_position.x - item.Position.x;
			const float dy = player_position.y - item.Position.y;
			if (player_can_heal &&
				dx * dx + dy * dy <= HEALING_ITEM_ATTRACTION_RADIUS_SQ)
			{
				item.Magnetized = true;
				item.MagnetSpeed = HEALING_ITEM_MAGNET_START_SPEED;
				item.Velocity = { 0.0f, 0.0f };
			}
			continue;
		}

		if (!player_can_heal)
		{
			item.Magnetized = false;
			item.MagnetSpeed = 0.0f;
			continue;
		}

		const float dx = player_position.x - item.Position.x;
		const float dy = player_position.y - item.Position.y;
		const float distance_sq = dx * dx + dy * dy;
		if (distance_sq <= HEALING_ITEM_ABSORB_RADIUS_SQ)
		{
			if (GamePlayer::Heal(HEALING_ITEM_HEAL_AMOUNT) > 0.0f)
			{
				item = HealingItem{};
			}
			continue;
		}

		const float distance = std::sqrt(distance_sq);
		item.MagnetSpeed = std::min(
			HEALING_ITEM_MAGNET_MAX_SPEED,
			item.MagnetSpeed + HEALING_ITEM_MAGNET_ACCELERATION * safe_delta_time);
		const float move_distance = std::min(
			item.MagnetSpeed * safe_delta_time,
			distance);
		item.Position.x += dx / distance * move_distance;
		item.Position.y += dy / distance * move_distance;
	}
}

void Draw()
{
	if (g_HealingItemTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	static std::array<SpriteInstance, HEALING_ITEM_MAX> item_instances;
	static std::array<SpriteInstance, HEALING_ITEM_MAX> glow_instances;
	int instance_count = 0;
	for (const HealingItem& item : g_HealingItems)
	{
		if (!item.Active)
		{
			continue;
		}

		const float pulse_phase = std::sin(item.Age * 6.5f);
		const float pulse = 1.0f + pulse_phase * 0.08f;
		const DirectX::XMFLOAT2 draw_position = {
			item.Position.x,
			item.Position.y +
				std::sin(item.Age * 4.2f) * HEALING_ITEM_BOB_AMOUNT,
		};
		glow_instances[instance_count] = {
			draw_position,
			{ HEALING_ITEM_GLOW_SIZE * pulse, HEALING_ITEM_GLOW_SIZE * pulse },
			0.0f,
			{ 0.18f, 1.0f, 0.28f, 0.34f + pulse_phase * 0.06f },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			1.0f,
		};
		item_instances[instance_count] = {
			draw_position,
			{ HEALING_ITEM_DRAW_SIZE * pulse, HEALING_ITEM_DRAW_SIZE * pulse },
			0.0f,
			{ 1.0f, 1.0f, 1.0f, 1.0f },
		};
		++instance_count;
	}

	if (instance_count <= 0)
	{
		return;
	}
	SpriteInstanced_DrawAdditiveUnlit(
		g_HealingItemTextureID,
		glow_instances.data(),
		instance_count);
	SpriteInstanced_Draw(
		g_HealingItemTextureID,
		item_instances.data(),
		instance_count);
}
}
