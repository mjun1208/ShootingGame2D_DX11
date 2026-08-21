#include "game_effect.h"

#include "sprite_instanced.h"
#include "texture.h"

#include <vector>

namespace
{
	constexpr wchar_t ELECTRIC_IMPACT_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/electric-impact/sprite-sheet.png";
	constexpr wchar_t WARM_EXPLOSION_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/warm-explosion/sprite-sheet.png";
	constexpr wchar_t VOID_IMPLOSION_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/void-implosion/sprite-sheet.png";
	constexpr wchar_t ENEMY_DEFEAT_SMOKE_TEXTURE_PATH[] =
		L"asset/texture/vfx/pvfx_foundry/smoke-puff/sprite-sheet.png";
	constexpr wchar_t SMOKE_POOF_TEXTURE_PATH[] =
		L"asset/texture/vfx/smoke-poof/smoke.png";
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
		96,
		96,
		15,
		5,
		0.05f,
		224.0f,
		224.0f,
		{ 0.0f, 18.0f }
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
			SpriteInstanced_Draw(
				m_Definitions[type_index].TextureID,
				instances.data(),
				static_cast<int>(instances.size()));
		}
	}
}

bool cGameEffectManager::Play(
	GameEffectType type,
	const DirectX::XMFLOAT2& position,
	float scale,
	const DirectX::XMFLOAT4& color)
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
	desc.Color = color;
	target->Play(desc);
	return true;
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
