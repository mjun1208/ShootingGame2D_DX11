#include "effect.h"

#include "texture.h"

#include <algorithm>

void cEffect::Play(const cEffectDesc& desc)
{
	m_Desc = desc;
	m_Desc.FrameWidth = std::max(m_Desc.FrameWidth, 1);
	m_Desc.FrameHeight = std::max(m_Desc.FrameHeight, 1);
	m_Desc.FrameCount = std::max(m_Desc.FrameCount, 1);
	m_Desc.FrameColumns = std::max(m_Desc.FrameColumns, 1);
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

void cEffect::Deactivate()
{
	m_IsActive = false;
}

bool cEffect::IsActive() const
{
	return m_IsActive;
}

int cEffect::GetTextureID() const
{
	return m_Desc.TextureID;
}

bool cEffect::BuildInstance(SpriteInstance& out_instance) const
{
	if (!m_IsActive || m_Desc.TextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}

	const DirectX::XMUINT2 texture_size = Texture_GetSize(m_Desc.TextureID);
	if (texture_size.x == 0 || texture_size.y == 0)
	{
		return false;
	}

	const int frame = GetCurrentFrame();
	const int frame_x = (frame % m_Desc.FrameColumns) * m_Desc.FrameWidth;
	const int frame_y = (frame / m_Desc.FrameColumns) * m_Desc.FrameHeight;
	out_instance = {
		m_Desc.Position,
		{ m_Desc.DrawWidth, m_Desc.DrawHeight },
		0.0f,
		m_Desc.Color,
		{
			static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
			static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
		},
		{
			static_cast<float>(m_Desc.FrameWidth) / static_cast<float>(texture_size.x),
			static_cast<float>(m_Desc.FrameHeight) / static_cast<float>(texture_size.y),
		},
	};
	return true;
}

int cEffect::GetCurrentFrame() const
{
	const int frame = static_cast<int>(m_ElapsedTime / m_Desc.FrameTime);
	return std::clamp(frame, 0, m_Desc.FrameCount - 1);
}
