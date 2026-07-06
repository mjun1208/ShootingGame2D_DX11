#include "enemy.h"

#include "collision.h"
#include "config.h"
#include "sprite.h"
#include "texture.h"

void cEnemy::Spawn(const DirectX::XMFLOAT2& position, float speed)
{
	m_Position = position;
	m_Speed = speed;
	m_HitPoint = 1.0f;
	m_DissolveTimer = 0.0f;
	m_State = State::Alive;
}

void cEnemy::Update(float delta_time)
{
	if (m_State == State::Inactive)
	{
		return;
	}

	if (m_State == State::Dying)
	{
		m_DissolveTimer += delta_time;
		if (m_DissolveTimer >= DISSOLVE_DURATION)
		{
			Deactivate();
		}
		return;
	}

	m_Position.y += m_Speed * delta_time;

	if (m_Position.y - HEIGHT * 0.5f > static_cast<float>(SCREEN_HEIGHT))
	{
		Deactivate();
	}
}

void cEnemy::Draw(int texture_id, int dissolve_noise_texture_id) const
{
	if (m_State == State::Inactive || texture_id == TEXTURE_INVALID_ID)
	{
		return;
	}

	if (m_State == State::Dying)
	{
		const float dissolve_amount = m_DissolveTimer / DISSOLVE_DURATION;
		Sprite_DrawDissolve(
			texture_id,
			dissolve_noise_texture_id,
			m_Position.x,
			m_Position.y,
			WIDTH,
			HEIGHT,
			dissolve_amount,
			DISSOLVE_EDGE_WIDTH,
			{ 1.0f, 0.35f, 0.05f, 1.8f });
		return;
	}

	Sprite_Draw(texture_id, m_Position.x, m_Position.y, WIDTH, HEIGHT);
}

void cEnemy::RegisterCollider(int owner_id) const
{
	if (!IsAlive())
	{
		return;
	}

	CollisionSystem_RegisterCircle(
		owner_id,
		CollisionLayer::Enemy,
		CollisionLayer::Player | CollisionLayer::PlayerBullet,
		m_Position,
		RADIUS);
}

void cEnemy::ApplyDamage(float damage)
{
	if (!IsAlive())
	{
		return;
	}

	m_HitPoint -= damage;
	if (m_HitPoint <= 0.0f)
	{
		m_HitPoint = 0.0f;
		m_DissolveTimer = 0.0f;
		m_State = State::Dying;
	}
}

void cEnemy::Deactivate()
{
	m_State = State::Inactive;
	m_DissolveTimer = 0.0f;
}

bool cEnemy::IsActive() const
{
	return m_State != State::Inactive;
}

bool cEnemy::IsAlive() const
{
	return m_State == State::Alive;
}

DirectX::XMFLOAT2 cEnemy::GetPosition() const
{
	return m_Position;
}