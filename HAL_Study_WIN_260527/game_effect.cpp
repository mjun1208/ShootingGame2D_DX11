#include "game_effect.h"

#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <vector>

namespace
{
	constexpr float COMBAT_EFFECT_OPACITY = 0.50f;
	constexpr wchar_t ELECTRIC_IMPACT_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/electric-impact/sprite-sheet.png";
	constexpr wchar_t WARM_EXPLOSION_TEXTURE_PATH[] =
		L"asset/texture/vfx/dryrainent/ppvfx-general-pack-1/blast_big.png";
	constexpr float WARM_EXPLOSION_VISIBLE_DIAMETER_RATIO = 20.0f / 32.0f;
	constexpr wchar_t VOID_IMPLOSION_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/void-implosion/sprite-sheet.png";
	constexpr wchar_t ENEMY_DEFEAT_SMOKE_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/smoke-puff/sprite-sheet.png";
	constexpr wchar_t SMOKE_POOF_TEXTURE_PATH[] =
		L"asset/texture/vfx/smoke-poof/smoke.png";
	constexpr wchar_t PIXEL_MAGIC_HIT_TEXTURE_PATH[] =
		L"asset/texture/vfx/codemanu/free-pixel-effects-pack/"
		L"5_magickahit_spritesheet.png";
	constexpr wchar_t SPRINT_DUST_TEXTURE_PATH[] =
		L"asset/texture/vfx/dryrainent/ppvfx-general-pack-1/dust_mid.png";
	constexpr wchar_t DASH_SLASH_HIT_BURST_TEXTURE_PATH[] =
		L"asset/texture/vfx/frostwindz/pixel-art-impacts/impact_2_bw.png";
	constexpr wchar_t DASH_SLASH_HIT_CUT_TEXTURE_PATH[] =
		L"asset/texture/vfx/frostwindz/pixel-art-slashes/"
		L"slash_2_color3_128.png";
	constexpr wchar_t ENEMY_WARRIOR_SLASH_TEXTURE_PATH[] =
		L"asset/texture/vfx/tbbk/pixel-sword-slash/"
		L"pixel_art_sword_slash_sprites.png";
}

cGameEffectManager& cGameEffectManager::GetInstance()
{
	static cGameEffectManager instance;
	return instance;
}

void cGameEffectManager::Initialize()
{
	if (m_IsInitialized)
	{
		return;
	}

	m_Definitions[static_cast<std::size_t>(GameEffectType::ElectricImpact)] = {
		ELECTRIC_IMPACT_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		96,
		96,
		14,
		5,
		0.05f,
		158.0f,
		158.0f,
		{ 0.0f, 10.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::WarmExplosion)] = {
		WARM_EXPLOSION_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		32,
		32,
		8,
		8,
		0.07f,
		224.0f,
		224.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::VoidImplosion)] = {
		VOID_IMPLOSION_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		96,
		96,
		14,
		5,
		0.05f,
		176.0f,
		176.0f,
		{ 0.0f, 4.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::EnemyDefeatSmoke)] = {
		ENEMY_DEFEAT_SMOKE_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		96,
		96,
		14,
		5,
		0.05f,
		148.0f,
		148.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::SmokePoof)] = {
		SMOKE_POOF_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		32,
		32,
		7,
		7,
		0.065f,
		128.0f,
		128.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::PixelMagicHit)] = {
		PIXEL_MAGIC_HIT_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		100,
		100,
		49,
		7,
		0.018f,
		138.0f,
		138.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::SprintDust)] = {
		SPRINT_DUST_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		32,
		32,
		7,
		7,
		0.045f,
		112.0f,
		112.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::DashSlashHitBurst)] = {
		DASH_SLASH_HIT_BURST_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		64,
		64,
		7,
		5,
		0.024f,
		126.0f,
		126.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::DashSlashHitCut)] = {
		DASH_SLASH_HIT_CUT_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		128,
		128,
		7,
		5,
		0.022f,
		164.0f,
		164.0f,
		{ 0.0f, 0.0f }
	};
	m_Definitions[static_cast<std::size_t>(GameEffectType::EnemyWarriorSlash)] = {
		ENEMY_WARRIOR_SLASH_TEXTURE_PATH,
		TEXTURE_INVALID_ID,
		64,
		47,
		9,
		3,
		0.035f,
		168.0f,
		124.0f,
		{ 0.0f, 0.0f }
	};

	auto set_light = [this](
		GameEffectType type,
		const DirectX::XMFLOAT3& color,
		float radius,
		float strength)
	{
		Definition& definition =
			m_Definitions[static_cast<std::size_t>(type)];
		definition.LightColor = color;
		definition.LightRadius = radius;
		definition.LightStrength = strength;
	};
	set_light(GameEffectType::ElectricImpact,
		{ 0.18f, 0.72f, 1.0f }, 230.0f, 1.05f);
	set_light(GameEffectType::WarmExplosion,
		{ 1.0f, 0.42f, 0.10f }, 190.0f, 1.35f);
	set_light(GameEffectType::VoidImplosion,
		{ 0.52f, 0.20f, 1.0f }, 210.0f, 0.90f);
	set_light(GameEffectType::SmokePoof,
		{ 1.0f, 0.56f, 0.14f }, 190.0f, 0.50f);
	set_light(GameEffectType::PixelMagicHit,
		{ 0.72f, 0.30f, 1.0f }, 155.0f, 0.68f);
	set_light(GameEffectType::DashSlashHitBurst,
		{ 0.16f, 0.62f, 1.0f }, 145.0f, 0.78f);
	set_light(GameEffectType::DashSlashHitCut,
		{ 0.18f, 0.48f, 1.0f }, 180.0f, 0.56f);
	set_light(GameEffectType::EnemyWarriorSlash,
		{ 1.0f, 0.06f, 0.02f }, 175.0f, 0.64f);

	for (Definition& definition : m_Definitions)
	{
		definition.TextureID = Texture_Load(definition.TexturePath, false);
	}

	Clear();
	m_IsInitialized = true;
}

void cGameEffectManager::Finalize()
{
	Clear();
	for (Definition& definition : m_Definitions)
	{
		Texture_Release(definition.TextureID);
		definition.TextureID = TEXTURE_INVALID_ID;
	}
	m_IsInitialized = false;
}

void cGameEffectManager::Update(float delta_time)
{
	for (cEffect& effect : m_Effects)
	{
		effect.Update(delta_time);
	}
}

void cGameEffectManager::Clear()
{
	for (cEffect& effect : m_Effects)
	{
		effect.Deactivate();
	}
	m_ReplaceIndex = 0;
}

void cGameEffectManager::Draw() const
{
	static std::array<std::vector<SpriteInstance>, TYPE_COUNT> batches;
	for (std::vector<SpriteInstance>& instances : batches)
	{
		instances.clear();
	}

	for (const cEffect& effect : m_Effects)
	{
		SpriteInstance instance{};
		if (effect.BuildInstance(instance))
		{
			instance.Color.w *= COMBAT_EFFECT_OPACITY;
			for (std::size_t type_index = 0; type_index < TYPE_COUNT; ++type_index)
			{
				if (m_Definitions[type_index].TextureID == effect.GetTextureID())
				{
					batches[type_index].push_back(instance);
					break;
				}
			}
		}
	}

	for (std::size_t type_index = 0; type_index < TYPE_COUNT; ++type_index)
	{
		const std::vector<SpriteInstance>& instances = batches[type_index];
		if (!instances.empty())
		{
			SpriteInstanced_DrawUnlit(
				m_Definitions[type_index].TextureID,
				instances.data(),
				static_cast<int>(instances.size()));
		}
	}
}

int cGameEffectManager::AppendPointLights(
	SpritePointLight* lights,
	int light_count,
	int capacity) const
{
	if (!lights || capacity <= 0)
	{
		return 0;
	}

	light_count = std::clamp(light_count, 0, capacity);
	for (const cEffect& effect : m_Effects)
	{
		if (light_count >= capacity)
		{
			break;
		}

		SpritePointLight light{};
		if (effect.BuildPointLight(light))
		{
			lights[light_count++] = light;
		}
	}
	return light_count;
}

bool cGameEffectManager::Play(
	GameEffectType type,
	const DirectX::XMFLOAT2& position,
	float scale,
	const DirectX::XMFLOAT4& color,
	float rotation)
{
	const Definition* definition = GetDefinition(type);
	if (!m_IsInitialized || !definition ||
		definition->TextureID == TEXTURE_INVALID_ID || scale <= 0.0f)
	{
		return false;
	}

	cEffect* target = nullptr;
	for (cEffect& effect : m_Effects)
	{
		if (!effect.IsActive())
		{
			target = &effect;
			break;
		}
	}
	if (!target)
	{
		target = &m_Effects[m_ReplaceIndex];
		m_ReplaceIndex = (m_ReplaceIndex + 1) % EFFECT_MAX;
	}

	cEffectDesc desc{};
	desc.Position = {
		position.x - definition->PivotOffset.x * scale,
		position.y - definition->PivotOffset.y * scale
	};
	desc.TextureID = definition->TextureID;
	desc.FrameWidth = definition->FrameWidth;
	desc.FrameHeight = definition->FrameHeight;
	desc.FrameCount = definition->FrameCount;
	desc.FrameColumns = definition->FrameColumns;
	desc.FrameTime = definition->FrameTime;
	desc.DrawWidth = definition->DrawWidth * scale;
	desc.DrawHeight = definition->DrawHeight * scale;
	desc.Rotation = rotation;
	desc.Color = color;
	desc.LightPosition = position;
	desc.LightColor = definition->LightColor;
	desc.LightRadius = definition->LightRadius * scale;
	desc.LightStrength = definition->LightStrength;
	target->Play(desc);
	return true;
}

bool cGameEffectManager::PlayAreaExplosion(
	const DirectX::XMFLOAT2& position,
	float radius)
{
	const Definition* definition = GetDefinition(GameEffectType::WarmExplosion);
	if (radius <= 0.0f || !definition || definition->DrawWidth <= 0.0f)
	{
		return false;
	}

	const float scale = radius * 2.0f /
		(definition->DrawWidth * WARM_EXPLOSION_VISIBLE_DIAMETER_RATIO);
	const DirectX::XMFLOAT2 centered_position = {
		position.x + definition->PivotOffset.x * scale,
		position.y + definition->PivotOffset.y * scale
	};
	return Play(GameEffectType::WarmExplosion, centered_position, scale);
}

void cGameEffectManager::PlayEnemyDefeat(const DirectX::XMFLOAT2& position)
{
	Play(
		GameEffectType::EnemyDefeatSmoke,
		position,
		0.78f,
		{ 0.72f, 0.78f, 0.88f, 0.86f });
}

const cGameEffectManager::Definition* cGameEffectManager::GetDefinition(
	GameEffectType type) const
{
	const std::size_t index = static_cast<std::size_t>(type);
	return index < m_Definitions.size() ? &m_Definitions[index] : nullptr;
}
