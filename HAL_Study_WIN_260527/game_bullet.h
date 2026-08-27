#ifndef GAME_BULLET_H
#define GAME_BULLET_H

#include <DirectXMath.h>

enum class BulletType
{
	Fireball = 0,
	Lightning,
	Ricochet,
	BezierHoming,
	OrbitBlade,
	Boomerang,
	Shotgun,
	MagicBlade,
	Count,
};

namespace GameBullet
{
	void Initialize();
	void Finalize();
	bool UnlockRandomWeapon();
	bool ConsumeUnlockedWeapon(BulletType& out_type);
	bool IsWeaponOwned(BulletType type);
	int GetWeaponTextureID(BulletType type);
	void IncreaseProjectileCount(BulletType type);
	void MultiplyAttackSpeed(BulletType type, float multiplier);
	void MultiplyDamage(BulletType type, float multiplier);
	void PlayFireballExplosionSound();
	bool Fire(
		const DirectX::XMFLOAT2& spawn_position,
		const DirectX::XMFLOAT2& target_position);
	void Update(float delta_time);
	void Clear();
	void Draw();
	void RegisterColliders();
}

#endif // !GAME_BULLET_H
