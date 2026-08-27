#ifndef ENEMY_H
#define ENEMY_H

#include <DirectXMath.h>

class cEnemy
{
public:
	static constexpr float WIDTH = 96.0f;
	static constexpr float HEIGHT = 96.0f;
	static constexpr float RADIUS = 24.0f;

	void Spawn(
		const DirectX::XMFLOAT2& position,
		float speed,
		float max_hit_point = 23.0f,
		float collision_radius = RADIUS,
		const DirectX::XMFLOAT2& map_collision_offset = { 0.0f, 0.0f },
		float map_collision_radius = 0.0f);
	void Update(
		float delta_time,
		const DirectX::XMFLOAT2& target_position,
		float chase_speed_scale = 1.0f);
	void ApplySeparation(const DirectX::XMFLOAT2& movement);
	void ApplyKnockback(const DirectX::XMFLOAT2& direction, float speed);
	void Draw(
		int texture_id,
		int texture_x,
		int texture_y,
		int texture_width,
		int texture_height,
		float draw_width = WIDTH,
		float draw_height = HEIGHT,
		bool source_faces_left = false) const;
	void RegisterCollider(int owner_id) const;
	void ApplyDamage(float damage);
	void SetHitPoint(float hit_point);
	void Deactivate();

	bool IsActive() const;
	bool IsAlive() const;
	bool IsFacingLeft() const;
	DirectX::XMFLOAT2 GetPosition() const;
	float GetCollisionRadius() const;
	DirectX::XMFLOAT2 GetMapCollisionCenter() const;
	float GetMapCollisionRadius() const;
	float GetHitPoint() const;
	float GetMaxHitPoint() const;
	float GetHitPointRatio() const;

private:
	enum class State
	{
		Inactive,
		Alive,
		Dying,
	};

	static constexpr float DISSOLVE_DURATION = 0.6f;
	static constexpr float KNOCKBACK_DECELERATION = 1350.0f;
	static constexpr float MAX_KNOCKBACK_SPEED = 620.0f;
	static constexpr float MAP_COLLISION_PADDING = 8.0f;

	void DecelerateKnockback(float delta_time);
	void MoveWithMapCollision(const DirectX::XMFLOAT2& movement);

	DirectX::XMFLOAT2 m_Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 m_KnockbackVelocity{ 0.0f, 0.0f };
	float m_Speed{ 0.0f };
	float m_HitPoint{ 23.0f };
	float m_MaxHitPoint{ 23.0f };
	float m_CollisionRadius{ RADIUS };
	DirectX::XMFLOAT2 m_MapCollisionOffset{ 0.0f, 0.0f };
	float m_MapCollisionRadius{ RADIUS };
	float m_DissolveTimer{ 0.0f };
	bool m_FacingLeft{ false };
	State m_State{ State::Inactive };
};

#endif // !ENEMY_H
