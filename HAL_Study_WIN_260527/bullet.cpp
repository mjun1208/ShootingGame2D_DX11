#include "bullet.h"

#include "config.h"
#include "sprite.h"

static constexpr float BULLET_SPEED = 900.0f;

void cBullet::Fire(const DirectX::XMFLOAT2& position)
{
	m_Position = position;
	m_IsActive = true;
}

void cBullet::Update(float delta_time)
{
	if (!m_IsActive)
	{
		return;
	}

	m_Position.y -= BULLET_SPEED * delta_time;

	if (m_Position.y + HEIGHT / 2.0f < 0.0f)
	{
		m_IsActive = false;
	}
}

void cBullet::Draw(int texture_id) const
{
	if (!m_IsActive)
	{
		return;
	}

	Sprite_Draw(texture_id, m_Position.x, m_Position.y, WIDTH, HEIGHT);
}

bool cBullet::IsActive() const
{
	return m_IsActive;
}
