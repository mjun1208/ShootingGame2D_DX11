#include "round_portal.h"

#include "sprite.h"
#include "texture.h"

#include <algorithm>

namespace
{
	constexpr wchar_t PORTAL_OPEN_TEXTURE_PATH[] =
		L"asset/texture/portal/portal_open.png";
	constexpr wchar_t PORTAL_IDLE_TEXTURE_PATH[] =
		L"asset/texture/portal/portal_idle.png";
	constexpr int PORTAL_FRAME_WIDTH = 32;
	constexpr int PORTAL_FRAME_HEIGHT = 32;
	constexpr int PORTAL_OPEN_COLUMNS = 4;
	constexpr int PORTAL_OPEN_FRAME_COUNT = 17;
	constexpr int PORTAL_IDLE_FRAME_COUNT = 5;
	constexpr float PORTAL_OPEN_FRAME_TIME = 0.055f;
	constexpr float PORTAL_IDLE_FRAME_TIME = 0.10f;
	constexpr float PORTAL_DRAW_SIZE = 160.0f;
	constexpr float PORTAL_INTERACTION_RADIUS = 120.0f;
	constexpr float PORTAL_PROMPT_OFFSET_Y = 112.0f;
}

void RoundPortal::Initialize(const DirectX::XMFLOAT2& position)
{
	m_OpenTextureID = Texture_Load(PORTAL_OPEN_TEXTURE_PATH, false);
	m_IdleTextureID = Texture_Load(PORTAL_IDLE_TEXTURE_PATH, false);
	Reset(position);
}

void RoundPortal::Finalize()
{
	Texture_Release(m_IdleTextureID);
	Texture_Release(m_OpenTextureID);
	m_IdleTextureID = TEXTURE_INVALID_ID;
	m_OpenTextureID = TEXTURE_INVALID_ID;
	m_IsOpen = false;
	m_IsActivated = false;
}

void RoundPortal::Reset(const DirectX::XMFLOAT2& position)
{
	m_Position = position;
	m_AnimationElapsedTime = 0.0f;
	m_IsOpen = false;
	m_IsActivated = false;
}

void RoundPortal::Update(float delta_time, bool open)
{
	if (open && !m_IsOpen)
	{
		m_AnimationElapsedTime = 0.0f;
		m_IsActivated = false;
	}

	m_IsOpen = open;
	if (m_IsOpen)
	{
		m_AnimationElapsedTime += std::max(delta_time, 0.0f);
	}
}

bool RoundPortal::CanInteract(
	const DirectX::XMFLOAT2& interactor_position) const
{
	if (!m_IsOpen || m_IsActivated)
	{
		return false;
	}

	const float dx = interactor_position.x - m_Position.x;
	const float dy = interactor_position.y - m_Position.y;
	return dx * dx + dy * dy <=
		PORTAL_INTERACTION_RADIUS * PORTAL_INTERACTION_RADIUS;
}

DirectX::XMFLOAT2 RoundPortal::GetInteractionPromptPosition() const
{
	return { m_Position.x, m_Position.y - PORTAL_PROMPT_OFFSET_Y };
}

void RoundPortal::Interact()
{
	if (m_IsOpen)
	{
		m_IsActivated = true;
	}
}

bool RoundPortal::ConsumeActivation()
{
	const bool activated = m_IsActivated;
	m_IsActivated = false;
	return activated;
}

void RoundPortal::Draw() const
{
	if (!m_IsOpen ||
		m_OpenTextureID == TEXTURE_INVALID_ID ||
		m_IdleTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const float open_duration =
		PORTAL_OPEN_FRAME_COUNT * PORTAL_OPEN_FRAME_TIME;
	int texture_id = m_IdleTextureID;
	int texture_x = 0;
	int texture_y = 0;
	if (m_AnimationElapsedTime < open_duration)
	{
		const int frame = std::clamp(
			static_cast<int>(m_AnimationElapsedTime / PORTAL_OPEN_FRAME_TIME),
			0,
			PORTAL_OPEN_FRAME_COUNT - 1);
		texture_id = m_OpenTextureID;
		texture_x = frame % PORTAL_OPEN_COLUMNS * PORTAL_FRAME_WIDTH;
		texture_y = frame / PORTAL_OPEN_COLUMNS * PORTAL_FRAME_HEIGHT;
	}
	else
	{
		const float idle_time = m_AnimationElapsedTime - open_duration;
		const int frame =
			static_cast<int>(idle_time / PORTAL_IDLE_FRAME_TIME) %
			PORTAL_IDLE_FRAME_COUNT;
		texture_x = frame * PORTAL_FRAME_WIDTH;
	}

	const bool lighting_was_enabled = Sprite_SetLightingEnabled(false);
	Sprite_DrawRegion(
		texture_id,
		m_Position.x,
		m_Position.y,
		PORTAL_DRAW_SIZE,
		PORTAL_DRAW_SIZE,
		texture_x,
		texture_y,
		PORTAL_FRAME_WIDTH,
		PORTAL_FRAME_HEIGHT,
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	Sprite_SetLightingEnabled(lighting_was_enabled);
}
