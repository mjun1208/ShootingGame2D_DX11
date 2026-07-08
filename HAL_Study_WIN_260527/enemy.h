#ifndef ENEMY_H
#define ENEMY_H

#include <DirectXMath.h>

class cEnemy
{
public:
	static constexpr float WIDTH = 96.0f;
	static constexpr float HEIGHT = 96.0f;
	static constexpr float RADIUS = 38.0f;

	void Spawn(const DirectX::XMFLOAT2& position, float speed);
	void Update(float delta_time, const DirectX::XMFLOAT2& target_position);
	void Draw(int texture_id, int dissolve_noise_texture_id) const;
	void RegisterCollider(int owner_id) const;
	void ApplyDamage(float damage);
	void Deactivate();

	bool IsActive() const;
	bool IsAlive() const;
	DirectX::XMFLOAT2 GetPosition() const;

private:
	enum class State
	{
		Inactive,
		Alive,
		Dying,
	};

	static constexpr float DISSOLVE_DURATION = 0.6f;
	static constexpr float DISSOLVE_EDGE_WIDTH = 0.12f;

	DirectX::XMFLOAT2 m_Position{ 0.0f, 0.0f };
	float m_Speed{ 0.0f };
	float m_HitPoint{ 1.0f };
	float m_DissolveTimer{ 0.0f };
	State m_State{ State::Inactive };
};

#endif // !ENEMY_H
