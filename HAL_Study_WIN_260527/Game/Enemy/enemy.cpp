#include "enemy.h"
#include "Constants/enemy_constants.h"

#include "collision.h"
#include "config.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "sprite.h"
#include "texture.h"

#include <algorithm>
#include <cmath>

namespace
{
	namespace EnemyTuning::Body
	{
		constexpr float DissolveDuration = 0.6f;
		constexpr float MaxKnockbackSpeed = 620.0f;
		constexpr float HitFlashDuration = 0.10f;
		constexpr float HitReactionDuration = 0.15f;
	} // namespace EnemyTuning::Body

} // namespace

void cEnemy::Spawn(const DirectX::XMFLOAT2& position, const SpawnSettings& settings)
{
	m_Position = position;
	m_KnockbackVelocity = { 0.0f, 0.0f };
	m_Speed = settings.Speed;
	m_MaxHitPoint = std::max(settings.MaxHitPoint, 1.0f);
	m_HitPoint = m_MaxHitPoint;
	m_CollisionRadius = std::max(settings.CollisionRadius, 1.0f);
	m_MapCollisionOffset = settings.MapCollisionOffset;
	m_MapCollisionRadius = settings.MapCollisionRadius > 0.0f ? settings.MapCollisionRadius : m_CollisionRadius;
	m_DissolveTimer = 0.0f;
	m_HitFlashTimer = 0.0f;
	m_HitReactionTimer = 0.0f;
	m_CutDirection = { 1.0f, 0.0f };
	m_CutFrameX = 0;
	m_CutFrameY = 0;
	m_IsCutDeath = false;
	m_FacingLeft = false;
	m_State = State::Alive;
}

void cEnemy::Update(float delta_time, const DirectX::XMFLOAT2& target_position, float chase_speed_scale)
{
	delta_time = std::max(delta_time, 0.0f);
	if (m_State == State::Inactive)
	{
		return;
	}

	const float safe_delta_time = delta_time;
	m_HitFlashTimer = std::max(m_HitFlashTimer - safe_delta_time, 0.0f);
	m_HitReactionTimer = std::max(m_HitReactionTimer - safe_delta_time, 0.0f);
	if (m_State == State::Dying)
	{
		const DirectX::XMFLOAT2 knockback_movement = {
			m_KnockbackVelocity.x * safe_delta_time,
			m_KnockbackVelocity.y * safe_delta_time,
		};
		MoveWithMapCollision(knockback_movement);
		DecelerateKnockback(safe_delta_time);

		m_DissolveTimer += safe_delta_time;
		if (m_DissolveTimer >= EnemyTuning::Body::DissolveDuration)
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
	const float knockback_speed = Length(m_KnockbackVelocity);
	const float chase_weight = std::max(0.0f, 1.0f - knockback_speed / EnemyTuning::Body::MaxKnockbackSpeed);
	const float movement_scale = std::clamp(chase_speed_scale, -1.6f, 2.0f);
	const DirectX::XMFLOAT2 chase_direction = NormalizeOr({ to_target_x, to_target_y }, { 0.0f, 0.0f });
	movement.x += chase_direction.x * m_Speed * chase_weight * movement_scale * safe_delta_time;
	movement.y += chase_direction.y * m_Speed * chase_weight * movement_scale * safe_delta_time;
	MoveWithMapCollision(movement);
	DecelerateKnockback(safe_delta_time);
}

void cEnemy::ApplySeparation(const DirectX::XMFLOAT2& movement)
{
	if (!IsAlive())
	{
		return;
	}

	MoveWithMapCollision(movement);
}

void cEnemy::MoveWithMapCollision(const DirectX::XMFLOAT2& movement)
{
	static constexpr float MapCollisionPadding = 8.0f;

	const DirectX::XMFLOAT2 map_center = GetMapCollisionCenter();
	const DirectX::XMFLOAT2 moved_center =
	    ProceduralMap_MoveActorCircle(map_center, movement, m_MapCollisionRadius + MapCollisionPadding);
	m_Position = {
		moved_center.x - m_MapCollisionOffset.x,
		moved_center.y - m_MapCollisionOffset.y,
	};
}

void cEnemy::ApplyKnockback(const DirectX::XMFLOAT2& direction, float speed)
{
	if (!IsAlive() || speed <= 0.0f)
	{
		return;
	}

	const float direction_length_sq = LengthSquared(direction);
	if (direction_length_sq <= 0.0001f)
	{
		return;
	}

	const DirectX::XMFLOAT2 normalized_direction = NormalizeOr(direction, { 0.0f, 0.0f });
	m_KnockbackVelocity.x += normalized_direction.x * speed;
	m_KnockbackVelocity.y += normalized_direction.y * speed;

	const float knockback_speed_sq = LengthSquared(m_KnockbackVelocity);
	if (knockback_speed_sq > EnemyTuning::Body::MaxKnockbackSpeed * EnemyTuning::Body::MaxKnockbackSpeed)
	{
		const float velocity_scale = EnemyTuning::Body::MaxKnockbackSpeed / std::sqrt(knockback_speed_sq);
		m_KnockbackVelocity.x *= velocity_scale;
		m_KnockbackVelocity.y *= velocity_scale;
	}
}

void cEnemy::BeginCutDeath(const DirectX::XMFLOAT2& slash_direction, int frame_x, int frame_y)
{
	if (m_State != State::Dying)
	{
		return;
	}

	m_CutDirection = NormalizeOr(slash_direction, { 1.0f, 0.0f });
	m_CutFrameX = frame_x;
	m_CutFrameY = frame_y;
	m_IsCutDeath = true;
}

void cEnemy::DecelerateKnockback(float delta_time)
{
	static constexpr float KnockbackDeceleration = 1350.0f;

	const float speed_sq = LengthSquared(m_KnockbackVelocity);
	if (speed_sq <= 0.0001f)
	{
		m_KnockbackVelocity = { 0.0f, 0.0f };
		return;
	}

	const float speed = std::sqrt(speed_sq);
	const float next_speed = std::max(0.0f, speed - KnockbackDeceleration * delta_time);
	const float velocity_scale = next_speed / speed;
	m_KnockbackVelocity.x *= velocity_scale;
	m_KnockbackVelocity.y *= velocity_scale;
}

void cEnemy::Draw(int texture_id, const SpriteRegion& source, const DirectX::XMFLOAT2& size,
                  bool source_faces_left) const
{
	if (m_State == State::Inactive || texture_id == TEXTURE_INVALID_ID)
	{
		return;
	}

	const float alpha = m_State == State::Dying ? 1.0f - m_DissolveTimer / EnemyTuning::Body::DissolveDuration : 1.0f;
	const bool flip_horizontal = m_FacingLeft != source_faces_left;
	Sprite_DrawRegion(texture_id, m_Position, { flip_horizontal ? -size.x : size.x, size.y }, source,
	                  { 1.0f, 1.0f, 1.0f, alpha });
}

void cEnemy::RegisterCollider(int owner_id) const
{
	if (!IsAlive())
	{
		return;
	}

	CollisionSystem_RegisterCircle(owner_id, CollisionLayer::Enemy,
	                               CollisionLayer::Player | CollisionLayer::PlayerBullet, m_Position,
	                               m_CollisionRadius);
}

void cEnemy::ApplyDamage(float damage)
{
	if (!IsAlive())
	{
		return;
	}

	m_HitPoint -= damage;
	m_HitFlashTimer = EnemyTuning::Body::HitFlashDuration;
	m_HitReactionTimer = EnemyTuning::Body::HitReactionDuration;
	if (m_HitPoint <= 0.0f)
	{
		m_HitPoint = 0.0f;
		m_DissolveTimer = 0.0f;
		m_State = State::Dying;
	}
}

float cEnemy::GetHitFlashAmount() const
{
	if (m_HitFlashTimer <= 0.0f)
	{
		return 0.0f;
	}

	const float remaining = Saturate(m_HitFlashTimer / EnemyTuning::Body::HitFlashDuration);
	// 처음 몇 프레임은 실루엣을 최대 밝기로 유지하고 빠르게 없앤다.
	// 연속 피격도 각각 구분되어 보이도록 한다.
	return Saturate(remaining * 1.65f);
}

float cEnemy::GetHitVisualScale() const
{
	if (m_HitReactionTimer <= 0.0f)
	{
		return 1.0f;
	}

	const float progress = Saturate(1.0f - m_HitReactionTimer / EnemyTuning::Body::HitReactionDuration);
	if (progress < 0.28f)
	{
		const float punch = progress / 0.28f;
		return 1.0f + std::sin(punch * DirectX::XM_PI) * 0.07f;
	}

	const float rebound = (progress - 0.28f) / 0.72f;
	return 1.0f - std::sin(rebound * DirectX::XM_PI) * 0.03f * (1.0f - rebound);
}

void cEnemy::SetHitPoint(float hit_point)
{
	if (m_State == State::Inactive)
	{
		return;
	}
	m_HitPoint = std::clamp(hit_point, 0.0f, m_MaxHitPoint);
	if (m_HitPoint <= 0.0f)
	{
		m_DissolveTimer = 0.0f;
		m_State = State::Dying;
	}
}

void cEnemy::Deactivate()
{
	m_State = State::Inactive;
	m_DissolveTimer = 0.0f;
	m_HitFlashTimer = 0.0f;
	m_HitReactionTimer = 0.0f;
	m_KnockbackVelocity = { 0.0f, 0.0f };
	m_IsCutDeath = false;
}

bool cEnemy::IsActive() const
{
	return m_State != State::Inactive;
}

bool cEnemy::IsAlive() const
{
	return m_State == State::Alive;
}

bool cEnemy::IsCutDeath() const
{
	return m_State == State::Dying && m_IsCutDeath;
}

bool cEnemy::IsFacingLeft() const
{
	return m_FacingLeft;
}

DirectX::XMFLOAT2 cEnemy::GetPosition() const
{
	return m_Position;
}

DirectX::XMFLOAT2 cEnemy::GetCutDirection() const
{
	return m_CutDirection;
}

float cEnemy::GetDeathProgress() const
{
	return m_State == State::Dying ? Saturate(m_DissolveTimer / EnemyTuning::Body::DissolveDuration) : 0.0f;
}

int cEnemy::GetCutFrameX() const
{
	return m_CutFrameX;
}

int cEnemy::GetCutFrameY() const
{
	return m_CutFrameY;
}

float cEnemy::GetCollisionRadius() const
{
	return m_CollisionRadius;
}

DirectX::XMFLOAT2 cEnemy::GetMapCollisionCenter() const
{
	return {
		m_Position.x + m_MapCollisionOffset.x,
		m_Position.y + m_MapCollisionOffset.y,
	};
}

float cEnemy::GetMapCollisionRadius() const
{
	return m_MapCollisionRadius;
}

float cEnemy::GetHitPoint() const
{
	return m_HitPoint;
}

float cEnemy::GetMaxHitPoint() const
{
	return m_MaxHitPoint;
}

float cEnemy::GetHitPointRatio() const
{
	return m_MaxHitPoint > 0.0f ? m_HitPoint / m_MaxHitPoint : 0.0f;
}
