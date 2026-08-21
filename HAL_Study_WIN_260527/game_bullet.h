#ifndef GAME_BULLET_H
#define GAME_BULLET_H

#include <DirectXMath.h>

enum class BulletType
{
	Fireball = 0,
	Lightning,
	Piercing,
	All,
	Count,
};

namespace GameBullet
{
	void Initialize();
	void Finalize();
	void SetType(BulletType type);
	BulletType GetType();
	void Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& direction);
	void Update(float delta_time);
	void Clear();
	void Draw();
	void RegisterColliders();
}

#endif // !GAME_BULLET_H
