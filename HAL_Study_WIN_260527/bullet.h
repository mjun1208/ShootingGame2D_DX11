#ifndef BULLET_H
#define BULLET_H

#include <DirectXMath.h>

class cBullet
{
public:
	static constexpr float WIDTH = 32.0f;
	static constexpr float HEIGHT = 78.0f;

	void Fire(const DirectX::XMFLOAT2& position);
	void Update(float delta_time);
	void Draw(int texture_id) const;

	bool IsActive() const;

private:
	DirectX::XMFLOAT2 m_Position{ 0.0f, 0.0f };
	bool m_IsActive{ false };
};

#endif // !BULLET_H
