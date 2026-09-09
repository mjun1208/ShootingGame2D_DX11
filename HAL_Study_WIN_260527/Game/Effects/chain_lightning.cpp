#include "Audio.h"
#include "chain_lightning.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "math_utils.h"
#include "random_utils.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	namespace ChainLightningTuning::Animation
	{
		constexpr int FrameCount = 4;
		constexpr float ArcLifetime = 0.22f;
		constexpr float ArcFadeStart = 0.12f;
	} // namespace ChainLightningTuning::Animation

} // namespace

void cChainLightning::Initialize()
{
	static constexpr const wchar_t* TexturePaths[ChainLightningTuning::Animation::FrameCount] = {
		L"asset/texture/lightning/chain_lightning_01.png",
		L"asset/texture/lightning/chain_lightning_02.png",
		L"asset/texture/lightning/chain_lightning_03.png",
		L"asset/texture/lightning/chain_lightning_04.png",
	};

	Clear();
	for (int i = 0; i < ChainLightningTuning::Animation::FrameCount; ++i)
	{
		m_LightningTextureIDs[i] = Texture_Load(TexturePaths[i], false);
	}
	m_DischargeAudioID = Audio_Load("asset/sound/pixabay-electric-discharge-386160.wav");
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
		Audio_Unload(m_DischargeAudioID);
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

bool cChainLightning::TryTrigger(int first_enemy_id, const DirectX::XMFLOAT2& first_hit_position, float attack_damage)
{
	static constexpr float ChainDamageMultiplier = 0.75f;
	static constexpr float ChainRange = 540.0f;
	static constexpr int MaxChainJumps = 6;

	if (attack_damage <= 0.0f)
	{
		return false;
	}

	int visited_enemy_ids[MaxChainJumps + 1]{};
	int visited_count = 1;
	visited_enemy_ids[0] = first_enemy_id;
	DirectX::XMFLOAT2 current_position = first_hit_position;
	const float chain_damage = attack_damage * ChainDamageMultiplier;
	cGameEffectManager::GetInstance().Play(GameEffectType::ElectricImpact, first_hit_position);
	if (m_DischargeAudioID >= 0)
	{
		Audio_Play(m_DischargeAudioID);
	}

	for (int jump = 0; jump < MaxChainJumps; ++jump)
	{
		int target_enemy_id = -1;
		DirectX::XMFLOAT2 target_position{};
		if (!GameEnemy::FindNearestAliveForChain(current_position, visited_enemy_ids, visited_count, ChainRange,
		                                         target_enemy_id, target_position))
		{
			break;
		}

		SpawnArc(current_position, target_position);
		cGameEffectManager::GetInstance().Play(GameEffectType::ElectricImpact, target_position);
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
		if (arc.Age >= ChainLightningTuning::Animation::ArcLifetime)
		{
			arc.IsActive = false;
			continue;
		}

		arc.FrameTimer += delta_time;
		while (arc.FrameTimer >= arc.FrameInterval)
		{
			arc.FrameTimer -= arc.FrameInterval;
			const int frame_step =
			    1 + static_cast<int>(Random01() * static_cast<float>(ChainLightningTuning::Animation::FrameCount - 1));
			arc.FrameIndex = (arc.FrameIndex + frame_step) % ChainLightningTuning::Animation::FrameCount;
			arc.FrameInterval = 0.032f + Random01() * 0.025f;
			arc.FrameBrightness = Random01() < 0.12f ? 0.32f + Random01() * 0.16f : 0.76f + Random01() * 0.28f;
		}
	}
}

void cChainLightning::Draw() const
{
	static std::array<std::vector<SpriteInstance>, ChainLightningTuning::Animation::FrameCount> frame_instances;
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
		const float distance = Distance(arc.Start, arc.End);
		if (distance <= 0.001f)
		{
			continue;
		}

		const float fade_ratio =
		    Saturate((arc.Age - ChainLightningTuning::Animation::ArcFadeStart) /
		             (ChainLightningTuning::Animation::ArcLifetime - ChainLightningTuning::Animation::ArcFadeStart));
		const float fade = (1.0f - fade_ratio) * (1.0f - fade_ratio);
		const float spawn_flash = 1.0f + 0.30f * Saturate(1.0f - arc.Age / 0.035f);
		const float width = std::clamp(distance * 0.21f, 66.0f, 118.0f);
		const int frame_index = std::clamp(arc.FrameIndex, 0, ChainLightningTuning::Animation::FrameCount - 1);

		frame_instances[frame_index].push_back({
		    { (arc.Start.x + arc.End.x) * 0.5f, (arc.Start.y + arc.End.y) * 0.5f },
		    { width, distance + 16.0f },
		    std::atan2(-dx, dy),
		    { 0.02f, 0.62f, 1.0f, fade * arc.FrameBrightness * spawn_flash },
		    { 0.0f, 0.0f },
		    { 1.0f, 1.0f },
		    0.0f,
		});
	}

	for (int frame = 0; frame < ChainLightningTuning::Animation::FrameCount; ++frame)
	{
		if (!frame_instances[frame].empty() && m_LightningTextureIDs[frame] != TEXTURE_INVALID_ID)
		{
			SpriteInstanced_DrawLightning(m_LightningTextureIDs[frame], frame_instances[frame].data(),
			                              static_cast<int>(frame_instances[frame].size()));
		}
	}
}

int cChainLightning::AppendPointLights(SpritePointLight* lights, int light_count, int capacity) const
{
	light_count = std::clamp(light_count, 0, std::max(capacity, 0));
	if (!lights || capacity <= 0)
	{
		return light_count;
	}

	for (const Arc& arc : m_Arcs)
	{
		if (!arc.IsActive || light_count >= capacity)
		{
			continue;
		}

		const float distance = Distance(arc.Start, arc.End);
		const float fade_ratio =
		    Saturate((arc.Age - ChainLightningTuning::Animation::ArcFadeStart) /
		             (ChainLightningTuning::Animation::ArcLifetime - ChainLightningTuning::Animation::ArcFadeStart));
		const float fade = (1.0f - fade_ratio) * (1.0f - fade_ratio);
		lights[light_count++] = {
			{ (arc.Start.x + arc.End.x) * 0.5f, (arc.Start.y + arc.End.y) * 0.5f },
			std::clamp(distance * 0.62f, 190.0f, 350.0f),
			1.08f * fade * arc.FrameBrightness,
			{ 0.10f, 0.66f, 1.0f },
		};
	}
	return light_count;
}

void cChainLightning::SpawnArc(const DirectX::XMFLOAT2& start, const DirectX::XMFLOAT2& end)
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
	target->FrameInterval = 0.032f + Random01() * 0.025f;
	target->FrameBrightness = 0.88f + Random01() * 0.16f;
	target->FrameIndex = std::min(static_cast<int>(Random01() * ChainLightningTuning::Animation::FrameCount),
	                              ChainLightningTuning::Animation::FrameCount - 1);

	const float distance = Distance(start, end);
	if (distance <= 0.001f)
	{
		target->IsActive = false;
		return;
	}
	target->IsActive = true;
}
