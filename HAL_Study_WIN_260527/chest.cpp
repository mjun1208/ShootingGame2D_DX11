#include "chest.h"

#include "game_effect.h"
#include "sprite.h"
#include "texture.h"

#include <algorithm>

namespace
{
	constexpr wchar_t CHEST_TEXTURE_PATH[] =
		L"asset/texture/chest/chest-sheet.png";
	constexpr int CHEST_FRAME_WIDTH = 48;
	constexpr int CHEST_FRAME_HEIGHT = 48;
	constexpr int CHEST_FIRST_OPEN_FRAME = 0;
	constexpr int CHEST_TEXTURE_Y = 0;
	constexpr int CHEST_FRAME_COUNT = 8;
	constexpr float CHEST_FRAME_TIME = 0.075f;
	constexpr float CHEST_DRAW_WIDTH = 144.0f;
	constexpr float CHEST_DRAW_HEIGHT = 144.0f;
	constexpr float CHEST_INTERACTION_RADIUS = 150.0f;
	constexpr float CHEST_PROMPT_MARGIN = -10.0f;
}

void Chest::Initialize(const DirectX::XMFLOAT2& position)
{
	m_TextureID = Texture_Load(CHEST_TEXTURE_PATH, false);
	Reset(position);
}

void Chest::Finalize()
{
	Texture_Release(m_TextureID);
	m_TextureID = TEXTURE_INVALID_ID;
	m_State = State::Gone;
}

void Chest::Reset(const DirectX::XMFLOAT2& position)
{
	m_Position = position;
	m_AnimationElapsedTime = 0.0f;
	m_State = State::Closed;
}

void Chest::Update(float delta_time)
{
	if (m_State != State::Opening)
	{
		return;
	}

	m_AnimationElapsedTime += std::max(delta_time, 0.0f);
	if (m_AnimationElapsedTime >= CHEST_FRAME_TIME * CHEST_FRAME_COUNT)
	{
		FinishOpening();
	}
}

bool Chest::CanInteract(
	const DirectX::XMFLOAT2& interactor_position) const
{
	if (m_State != State::Closed)
	{
		return false;
	}

	const float dx = interactor_position.x - m_Position.x;
	const float dy = interactor_position.y - m_Position.y;
	return dx * dx + dy * dy <=
		CHEST_INTERACTION_RADIUS * CHEST_INTERACTION_RADIUS;
}

DirectX::XMFLOAT2 Chest::GetInteractionPromptPosition() const
{
	return {
		m_Position.x,
		m_Position.y - CHEST_DRAW_HEIGHT * 0.5f - CHEST_PROMPT_MARGIN
	};
}

void Chest::Interact()
{
	if (m_State != State::Closed)
	{
		return;
	}

	m_AnimationElapsedTime = 0.0f;
	m_State = State::Opening;
}

void Chest::Draw() const
{
	if (m_State == State::Gone || m_TextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	int frame = 0;
	if (m_State == State::Opening)
	{
		frame = std::clamp(
			static_cast<int>(m_AnimationElapsedTime / CHEST_FRAME_TIME),
			0,
			CHEST_FRAME_COUNT - 1);
	}

	Sprite_DrawRegion(
		m_TextureID,
		m_Position.x,
		m_Position.y,
		CHEST_DRAW_WIDTH,
		CHEST_DRAW_HEIGHT,
		(CHEST_FIRST_OPEN_FRAME + frame) * CHEST_FRAME_WIDTH,
		CHEST_TEXTURE_Y,
		CHEST_FRAME_WIDTH,
		CHEST_FRAME_HEIGHT,
		{ 1.0f, 1.0f, 1.0f, 1.0f });
}

bool Chest::IsGone() const
{
	return m_State == State::Gone;
}

void Chest::FinishOpening()
{
	m_State = State::Gone;
	cGameEffectManager::GetInstance().Play(
		GameEffectType::SmokePoof,
		{ m_Position.x, m_Position.y - 4.0f },
		1.5f);
}
