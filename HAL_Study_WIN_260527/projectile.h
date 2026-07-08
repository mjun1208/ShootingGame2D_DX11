#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "collision.h"

#include <DirectXMath.h>

constexpr int PROJECTILE_INVALID_ID = -1;
constexpr int PROJECTILE_MAX = 8192;

struct cProjectileDesc
{
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Velocity{ 0.0f, 0.0f };
	float Radius{ 8.0f };
	float Width{ 16.0f };
	float Height{ 16.0f };
	float Rotation{ 0.0f };
	float Damage{ 1.0f };
	float LifeTime{ 0.0f };
	int TextureID{ -1 };
	int OwnerID{ PROJECTILE_INVALID_ID };
	CollisionLayer Layer{ CollisionLayer::PlayerBullet };
	CollisionLayer HitMask{ CollisionLayer::Enemy };
	bool UsesTrail{ false };
	int TrailTextureID{ -1 };
	float TrailEmitInterval{ 0.02f };
	float TrailWidth{ 0.0f };
	float TrailLength{ 0.0f };
	float TrailOffset{ 0.0f };
	float TrailLifeTime{ 0.18f };
	float TrailStartScale{ 0.85f };
	float TrailEndScale{ 0.15f };
	DirectX::XMFLOAT4 TrailColor{ 1.0f, 1.0f, 1.0f, 0.35f };
};

struct cProjectile
{
	bool IsActive{ false };
	DirectX::XMFLOAT2 Position{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 Velocity{ 0.0f, 0.0f };
	float Radius{ 8.0f };
	float Width{ 16.0f };
	float Height{ 16.0f };
	float Rotation{ 0.0f };
	float Damage{ 1.0f };
	float Age{ 0.0f };
	float LifeTime{ 0.0f };
	int TextureID{ -1 };
	int OwnerID{ PROJECTILE_INVALID_ID };
	CollisionLayer Layer{ CollisionLayer::PlayerBullet };
	CollisionLayer HitMask{ CollisionLayer::Enemy };
	bool UsesTrail{ false };
	int TrailTextureID{ -1 };
	float TrailEmitInterval{ 0.02f };
	float TrailEmitTimer{ 0.0f };
	float TrailWidth{ 0.0f };
	float TrailLength{ 0.0f };
	float TrailOffset{ 0.0f };
	float TrailLifeTime{ 0.18f };
	float TrailStartScale{ 0.85f };
	float TrailEndScale{ 0.15f };
	DirectX::XMFLOAT4 TrailColor{ 1.0f, 1.0f, 1.0f, 0.35f };
};

void ProjectileSystem_Initialize();
void ProjectileSystem_Finalize();
void ProjectileSystem_Clear();
int ProjectileSystem_Fire(const cProjectileDesc& desc);
void ProjectileSystem_Update(float delta_time);
void ProjectileSystem_Draw();
void ProjectileSystem_RegisterColliders();
void ProjectileSystem_Deactivate(int projectile_id);
bool ProjectileSystem_IsActive(int projectile_id);
const cProjectile* ProjectileSystem_GetProjectile(int projectile_id);
int ProjectileSystem_GetActiveCount();
int ProjectileSystem_GetCapacity();

#endif // !PROJECTILE_H
