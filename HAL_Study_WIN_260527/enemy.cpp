#include "enemy.h"

#include "collision.h"
#include "config.h"
#include "procedural_map.h"
#include "sprite.h"
#include "texture.h"

#include <algorithm>
#include <cmath>

void cEnemy::Spawn(
	const DirectX::XMFLOAT2& position,
	float speed,
	float max_hit_point,
	float collision_radius)
{
	m_Position = position;
	m_KnockbackVelocity = { 0.0f, 0.0f };
	m_Speed = speed;
	m_HitPoint = std::max(max_hit_point, 1.0f);
	m_CollisionRadius = std::max(collision_radius, 1.0f);
	m_DissolveTimer = 0.0f;
	m_FacingLeft = false;
	m_State = State::Alive;
}

void cEnemy::Update(float delta_time, const DirectX::XMFLOAT2& target_position)
{
	if (m_State == State::Inactive)
	{
		return;
	}

	const float safe_delta_time = std::max(delta_time, 0.0f);
	if (m_State == State::Dying)
	{
		const DirectX::XMFLOAT2 knockback_movement = {
			m_KnockbackVelocity.x * safe_delta_time,
			m_KnockbackVelocity.y * safe_delta_time,
		};
		m_Position = ProceduralMap_MoveActorCircle(
			m_Position, knockback_movement, m_CollisionRadius);
		DecelerateKnockback(safe_delta_time);

		m_DissolveTimer += safe_delta_time;
		if (m_DissolveTimer >= DISSOLVE_DURATION)
		{
			Deactivate();
		}
		return;
	}

	const float to_target_x = target_position.x - m_Position.x;
	const float to_target_y = target_position.y - m_Position.y;
	if (std::abs(to_target_x) > 0.001f)
	{
		m_FacingLeft = to_target_x < 0.0f;
	}
	DirectX::XMFLOAT2 movement = {
		m_KnockbackVelocity.x * safe_delta_time,
		m_KnockbackVelocity.y * safe_delta_time,
	};
	const float knockback_speed = std::sqrt(
		m_KnockbackVelocity.x * m_KnockbackVelocity.x +
		m_KnockbackVelocity.y * m_KnockbackVelocity.y);
	const float chase_weight = std::max(0.0f, 1.0f - knockback_speed / MAX_KNOCKBACK_SPEED);
	const float distance_sq = to_target_x * to_target_x + to_target_y * to_target_y;
	if (distance_sq > 0.0001f)
	{
		const float inv_distance = 1.0f / std::sqrt(distance_sq);
		movement.x +=
			to_target_x * inv_distance * m_Speed * chase_weight * safe_delta_time;
		movement.y +=
			to_target_y * inv_distance * m_Speed * chase_weight * safe_delta_time;
	}
	m_Position = ProceduralMap_MoveActorCircle(m_Position, movement, m_CollisionRadius);
	DecelerateKnockback(safe_delta_time);
}

void cEnemy::ApplySeparation(const DirectX::XMFLOAT2& movement)
{
	if (!IsAlive())
	{
		return;
	}

	m_Position = ProceduralMap_MoveActorCircle(m_Position, movement, m_CollisionRadius);
}

void cEnemy::ApplyKnockback(const DirectX::XMFLOAT2& direction, float speed)
{
	if (!IsAlive() || speed <= 0.0f)
	{
		return;
	}

	const float direction_length_sq =
		direction.x * direction.x + direction.y * direction.y;
	if (direction_length_sq <= 0.0001f)
	{
		return;
	}

	const float inv_direction_length = 1.0f / std::sqrt(direction_length_sq);
	m_KnockbackVelocity.x += direction.x * inv_direction_length * speed;
	m_KnockbackVelocity.y += direction.y * inv_direction_length * speed;

	const float knockback_speed_sq =
		m_KnockbackVelocity.x * m_KnockbackVelocity.x +
		m_KnockbackVelocity.y * m_KnockbackVelocity.y;
	if (knockback_speed_sq > MAX_KNOCKBACK_SPEED * MAX_KNOCKBACK_SPEED)
	{
		const float velocity_scale = MAX_KNOCKBACK_SPEED / std::sqrt(knockback_speed_sq);
		m_KnockbackVelocity.x *= velocity_scale;
		m_KnockbackVelocity.y *= velocity_scale;
	}
}

void cEnemy::DecelerateKnockback(float delta_time)
{
	const float speed_sq =
		m_KnockbackVelocity.x * m_KnockbackVelocity.x +
		m_KnockbackVelocity.y * m_KnockbackVelocity.y;
	if (speed_sq <= 0.0001f)
	{
		m_KnockbackVelocity = { 0.0f, 0.0f };
		return;
	}

	const float speed = std::sqrt(speed_sq);
	const float next_speed = std::max(0.0f, speed - KNOCKBACK_DECELERATION * delta_time);
	const float velocity_scale = next_speed / speed;
	m_KnockbackVelocity.x *= velocity_scale;
	m_KnockbackVelocity.y *= velocity_scale;
}

void cEnemy::Draw(
	int texture_id,
	int texture_x,
	int texture_y,
	int texture_width,
	int texture_height,
	float draw_width,
	float draw_height,
	bool source_faces_left) const
{
	if (m_State == State::Inactive || texture_id == TEXTURE_INVALID_ID)
	{
		return;
	}

	const float alpha = m_State == State::Dying ?
		1.0f - m_DissolveTimer / DISSOLVE_DURATION : 1.0f;
	const bool flip_horizontal = m_FacingLeft != source_faces_left;
	Sprite_DrawRegion(
		texture_id,
		m_Position.x,
		m_Position.y,
		flip_horizontal ? -draw_width : draw_width,
		draw_height,
		texture_x,
		texture_y,
		texture_width,
		texture_height,
		{ 1.0f, 1.0f, 1.0f, alpha });
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
		m_CollisionRadius);
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
	m_KnockbackVelocity = { 0.0f, 0.0f };
}

bool cEnemy::IsActive() const
{
	return m_State != State::Inactive;
}

bool cEnemy::IsAlive() const
{
	return m_State == State::Alive;
}

bool cEnemy::IsFacingLeft() const
{
	return m_FacingLeft;
}

DirectX::XMFLOAT2 cEnemy::GetPosition() const
{
	return m_Position;
}

float cEnemy::GetCollisionRadius() const
{
	return m_CollisionRadius;
}
