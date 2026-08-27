#include "chain_lightning.h"

#include "Audio.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	constexpr int MAX_CHAIN_JUMPS = 6;
	constexpr float CHAIN_RANGE = 540.0f;
	constexpr float CHAIN_DAMAGE_MULTIPLIER = 0.75f;
	constexpr int LIGHTNING_FRAME_COUNT = 4;
	constexpr float ARC_LIFETIME = 0.22f;
	constexpr float ARC_FADE_START = 0.12f;
	constexpr const char* DISCHARGE_SOUND_PATH =
		"asset/sound/pixabay-electric-discharge-386160.wav";
	constexpr const wchar_t* LIGHTNING_TEXTURE_PATHS[LIGHTNING_FRAME_COUNT] = {
		L"asset/texture/lightning/chain_lightning_01.png",
		L"asset/texture/lightning/chain_lightning_02.png",
		L"asset/texture/lightning/chain_lightning_03.png",
		L"asset/texture/lightning/chain_lightning_04.png",
	};
}

void cChainLightning::Initialize()
{
	Clear();
	m_RandomState = 0x4C11DB7u;
	for (int i = 0; i < LIGHTNING_FRAME_COUNT; ++i)
	{
		m_LightningTextureIDs[i] = Texture_Load(
			LIGHTNING_TEXTURE_PATHS[i], false);
	}
	m_DischargeAudioID = LoadAudio(DISCHARGE_SOUND_PATH);
}

void cChainLightning::Finalize()
{
	Clear();
	for (int& texture_id : m_LightningTextureIDs)
	{
		Texture_Release(texture_id);
		texture_id = TEXTURE_INVALID_ID;
	}
	if (m_DischargeAudioID >= 0)
	{
		UnloadAudio(m_DischargeAudioID);
		m_DischargeAudioID = -1;
	}
}

void cChainLightning::Clear()
{
	for (Arc& arc : m_Arcs)
	{
		arc = Arc{};
	}
	m_ReplaceIndex = 0;
}

bool cChainLightning::TryTrigger(
	int first_enemy_id,
	const DirectX::XMFLOAT2& first_hit_position,
	float attack_damage)
{
	if (attack_damage <= 0.0f)
	{
		return false;
	}

	int visited_enemy_ids[MAX_CHAIN_JUMPS + 1]{};
	int visited_count = 1;
	visited_enemy_ids[0] = first_enemy_id;
	DirectX::XMFLOAT2 current_position = first_hit_position;
	const float chain_damage = attack_damage * CHAIN_DAMAGE_MULTIPLIER;
	cGameEffectManager::GetInstance().Play(
		GameEffectType::ElectricImpact,
		first_hit_position);
	if (m_DischargeAudioID >= 0)
	{
		PlayAudio(m_DischargeAudioID);
	}

	for (int jump = 0; jump < MAX_CHAIN_JUMPS; ++jump)
	{
		int target_enemy_id = -1;
		DirectX::XMFLOAT2 target_position{};
		if (!GameEnemy::FindNearestAliveForChain(
			current_position,
			visited_enemy_ids,
			visited_count,
			CHAIN_RANGE,
			target_enemy_id,
			target_position))
		{
			break;
		}

		SpawnArc(current_position, target_position);
		cGameEffectManager::GetInstance().Play(
			GameEffectType::ElectricImpact,
			target_position);
		GameEnemy::ApplyChainLightningDamage(target_enemy_id, chain_damage);
		visited_enemy_ids[visited_count++] = target_enemy_id;
		current_position = target_position;
	}

	return visited_count > 1;
}

void cChainLightning::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	for (Arc& arc : m_Arcs)
	{
		if (!arc.IsActive)
		{
			continue;
		}

		arc.Age += delta_time;
		if (arc.Age >= ARC_LIFETIME)
		{
			arc.IsActive = false;
			continue;
		}

		arc.FrameTimer += delta_time;
		while (arc.FrameTimer >= arc.FrameInterval)
		{
			arc.FrameTimer -= arc.FrameInterval;
			const int frame_step = 1 + static_cast<int>(
				NextRandom01() * static_cast<float>(LIGHTNING_FRAME_COUNT - 1));
			arc.FrameIndex =
				(arc.FrameIndex + frame_step) % LIGHTNING_FRAME_COUNT;
			arc.FrameInterval = 0.032f + NextRandom01() * 0.025f;
			arc.FrameBrightness = NextRandom01() < 0.12f ?
				0.32f + NextRandom01() * 0.16f :
				0.76f + NextRandom01() * 0.28f;
		}
	}
}

void cChainLightning::Draw() const
{
	static std::array<std::vector<SpriteInstance>, LIGHTNING_FRAME_COUNT>
		frame_instances;
	for (std::vector<SpriteInstance>& instances : frame_instances)
	{
		if (instances.capacity() < ARC_MAX)
		{
			instances.reserve(ARC_MAX);
		}
		instances.clear();
	}

	for (const Arc& arc : m_Arcs)
	{
		if (!arc.IsActive)
		{
			continue;
		}

		const float dx = arc.End.x - arc.Start.x;
		const float dy = arc.End.y - arc.Start.y;
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance <= 0.001f)
		{
			continue;
		}

		const float fade_ratio = std::clamp(
			(arc.Age - ARC_FADE_START) / (ARC_LIFETIME - ARC_FADE_START),
			0.0f, 1.0f);
		const float fade = (1.0f - fade_ratio) * (1.0f - fade_ratio);
		const float spawn_flash = 1.0f +
			0.30f * std::clamp(1.0f - arc.Age / 0.035f, 0.0f, 1.0f);
		const float width = std::clamp(distance * 0.21f, 66.0f, 118.0f);
		const int frame_index = std::clamp(
			arc.FrameIndex, 0, LIGHTNING_FRAME_COUNT - 1);

		frame_instances[frame_index].push_back({
			{ (arc.Start.x + arc.End.x) * 0.5f,
			  (arc.Start.y + arc.End.y) * 0.5f },
			{ width, distance + 16.0f },
			std::atan2(-dx, dy),
			{ 0.02f, 0.62f, 1.0f,
			  fade * arc.FrameBrightness * spawn_flash },
			{ 0.0f, 0.0f },
			{ 1.0f, 1.0f },
			0.0f,
		});
	}

	for (int frame = 0; frame < LIGHTNING_FRAME_COUNT; ++frame)
	{
		if (!frame_instances[frame].empty() &&
			m_LightningTextureIDs[frame] != TEXTURE_INVALID_ID)
		{
			SpriteInstanced_DrawLightning(
				m_LightningTextureIDs[frame],
				frame_instances[frame].data(),
				static_cast<int>(frame_instances[frame].size()));
		}
	}
}

int cChainLightning::AppendPointLights(
	SpritePointLight* lights,
	int light_count,
	int capacity) const
{
	if (!lights || capacity <= 0)
	{
		return 0;
	}

	light_count = std::clamp(light_count, 0, capacity);
	for (const Arc& arc : m_Arcs)
	{
		if (!arc.IsActive || light_count >= capacity)
		{
			continue;
		}

		const float dx = arc.End.x - arc.Start.x;
		const float dy = arc.End.y - arc.Start.y;
		const float distance = std::sqrt(dx * dx + dy * dy);
		const float fade_ratio = std::clamp(
			(arc.Age - ARC_FADE_START) / (ARC_LIFETIME - ARC_FADE_START),
			0.0f, 1.0f);
		const float fade = (1.0f - fade_ratio) * (1.0f - fade_ratio);
		lights[light_count++] = {
			{ (arc.Start.x + arc.End.x) * 0.5f,
			  (arc.Start.y + arc.End.y) * 0.5f },
			std::clamp(distance * 0.62f, 190.0f, 350.0f),
			1.08f * fade * arc.FrameBrightness,
			{ 0.10f, 0.66f, 1.0f },
		};
	}
	return light_count;
}

float cChainLightning::NextRandom01()
{
	m_RandomState ^= m_RandomState << 13;
	m_RandomState ^= m_RandomState >> 17;
	m_RandomState ^= m_RandomState << 5;
	return static_cast<float>(m_RandomState & 0x00FFFFFFu) /
		static_cast<float>(0x01000000u);
}

void cChainLightning::SpawnArc(
	const DirectX::XMFLOAT2& start,
	const DirectX::XMFLOAT2& end)
{
	Arc* target = nullptr;
	for (Arc& arc : m_Arcs)
	{
		if (!arc.IsActive)
		{
			target = &arc;
			break;
		}
	}

	if (!target)
	{
		target = &m_Arcs[m_ReplaceIndex];
		m_ReplaceIndex = (m_ReplaceIndex + 1) % ARC_MAX;
	}

	target->Start = start;
	target->End = end;
	target->Age = 0.0f;
	target->FrameTimer = 0.0f;
	target->FrameInterval = 0.032f + NextRandom01() * 0.025f;
	target->FrameBrightness = 0.88f + NextRandom01() * 0.16f;
	target->FrameIndex = std::min(
		static_cast<int>(NextRandom01() * LIGHTNING_FRAME_COUNT),
		LIGHTNING_FRAME_COUNT - 1);

	const float dx = end.x - start.x;
	const float dy = end.y - start.y;
	const float distance = std::sqrt(dx * dx + dy * dy);
	if (distance <= 0.001f)
	{
		target->IsActive = false;
		return;
	}
	target->IsActive = true;
}
