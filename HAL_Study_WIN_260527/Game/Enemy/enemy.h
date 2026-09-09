#ifndef ENEMY_H
#define ENEMY_H

#include "sprite_region.h"
#include <DirectXMath.h>

class cEnemy
{
  public:
	static constexpr float WIDTH = 96.0f;
	static constexpr float HEIGHT = 96.0f;
	static constexpr float RADIUS = 24.0f;

	struct SpawnSettings
	{
		float Speed{ 0.0f };
		float MaxHitPoint{ 23.0f };
		float CollisionRadius{ RADIUS };
		DirectX::XMFLOAT2 MapCollisionOffset{ 0.0f, 0.0f };
		float MapCollisionRadius{ 0.0f };
	};

	void Spawn(const DirectX::XMFLOAT2& position, const SpawnSettings& settings);
	void Update(float delta_time, const DirectX::XMFLOAT2& target_position, float chase_speed_scale = 1.0f);
	void ApplySeparation(const DirectX::XMFLOAT2& movement);
	void ApplyKnockback(const DirectX::XMFLOAT2& direction, float speed);
	void BeginCutDeath(const DirectX::XMFLOAT2& slash_direction, int frame_x, int frame_y);
	void Draw(int texture_id, const SpriteRegion& source,
	          const DirectX::XMFLOAT2& size = { WIDTH, HEIGHT }, bool source_faces_left = false) const;
	void RegisterCollider(int owner_id) const;
	void ApplyDamage(float damage);
	void SetHitPoint(float hit_point);
	void Deactivate();

	bool IsActive() const;
	bool IsAlive() const;
	bool IsCutDeath() const;
	bool IsFacingLeft() const;
	DirectX::XMFLOAT2 GetPosition() const;
	DirectX::XMFLOAT2 GetCutDirection() const;
	float GetDeathProgress() const;
	int GetCutFrameX() const;
	int GetCutFrameY() const;
	float GetCollisionRadius() const;
	DirectX::XMFLOAT2 GetMapCollisionCenter() const;
	float GetMapCollisionRadius() const;
	float GetHitPoint() const;
	float GetMaxHitPoint() const;
	float GetHitPointRatio() const;
	float GetHitFlashAmount() const;
	float GetHitVisualScale() const;

  private:
	enum class State
	{
		Inactive,
		Alive,
		Dying,
	};

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
	float m_HitFlashTimer{ 0.0f };
	float m_HitReactionTimer{ 0.0f };
	DirectX::XMFLOAT2 m_CutDirection{ 1.0f, 0.0f };
	int m_CutFrameX{ 0 };
	int m_CutFrameY{ 0 };
	bool m_IsCutDeath{ false };
	bool m_FacingLeft{ false };
	State m_State{ State::Inactive };
};

#endif // !ENEMY_H
