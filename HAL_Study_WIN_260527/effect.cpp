#include "effect.h"

#include "sprite.h"
#include "texture.h"

#include <algorithm>

void cEffect::Play(const cEffectDesc& desc)
{
	m_Desc = desc;
	m_Desc.FrameWidth = std::max(m_Desc.FrameWidth, 1);
	m_Desc.FrameHeight = std::max(m_Desc.FrameHeight, 1);
	m_Desc.FrameCount = std::max(m_Desc.FrameCount, 1);
	m_Desc.FrameTime = std::max(m_Desc.FrameTime, 0.001f);
	m_Desc.DrawWidth = m_Desc.DrawWidth > 0.0f ? m_Desc.DrawWidth : static_cast<float>(m_Desc.FrameWidth);
	m_Desc.DrawHeight = m_Desc.DrawHeight > 0.0f ? m_Desc.DrawHeight : static_cast<float>(m_Desc.FrameHeight);
	m_ElapsedTime = 0.0f;
	m_IsActive = true;
}

void cEffect::Update(float delta_time)
{
	if (!m_IsActive)
	{
		return;
	}

	m_ElapsedTime += delta_time;
	if (m_ElapsedTime >= m_Desc.FrameTime * static_cast<float>(m_Desc.FrameCount))
	{
		Deactivate();
	}
}

void cEffect::Draw() const
{
	if (!m_IsActive || m_Desc.TextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const int frame = GetCurrentFrame();
	Sprite_Draw(
		m_Desc.TextureID,
		m_Desc.Position.x,
		m_Desc.Position.y,
		m_Desc.DrawWidth,
		m_Desc.DrawHeight,
		frame * m_Desc.FrameWidth,
		0,
		m_Desc.FrameWidth,
		m_Desc.FrameHeight,
		m_Desc.Color);
}

void cEffect::Deactivate()
{
	m_IsActive = false;
}

bool cEffect::IsActive() const
{
	return m_IsActive;
}

int cEffect::GetCurrentFrame() const
{
	const int frame = static_cast<int>(m_ElapsedTime / m_Desc.FrameTime);
	return std::clamp(frame, 0, m_Desc.FrameCount - 1);
}
