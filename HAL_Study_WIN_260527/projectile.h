#ifndef PROJECTILE_H
#define PROJECTILE_H

#include "collision.h"

#include <DirectXMath.h>
#include <array>
#include <cstdint>

constexpr int PROJECTILE_INVALID_ID = -1;
constexpr int PROJECTILE_MAX = 8192;
constexpr int PROJECTILE_HIT_HISTORY_MAX = 16;

enum class ProjectileHitBehavior : std::uint8_t
{
	Stop,
	Area,
	ChainLightning,
	Pierce,
	PersistentPierce,
};

enum class ProjectileMotionBehavior : std::uint8_t
{
	Linear,
	Boomerang,
	OrbitOwner,
	MagicBlade,
};

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
	ProjectileHitBehavior HitBehavior{ ProjectileHitBehavior::Stop };
	float AreaRadius{ 0.0f };
	int MaxTargetHits{ 1 };
	int MaxBounces{ 0 };
	int AliveLimitGroup{ PROJECTILE_INVALID_ID };
	int MaxAliveInGroup{ 0 };
	bool UsesBezierHoming{ false };
	float BezierCurveStrength{ 0.0f };
	float BezierCurveDirection{ 1.0f };
	ProjectileMotionBehavior MotionBehavior{ ProjectileMotionBehavior::Linear };
	float BoomerangReturnTime{ 0.0f };
	float OrbitRadius{ 0.0f };
	float OrbitAngularSpeed{ 0.0f };
	float OrbitPhase{ 0.0f };
	float SpinSpeed{ 0.0f };
	float RepeatHitInterval{ 0.0f };
	float MagicBladeSummonTime{ 0.0f };
	float MagicBladeReadyDelay{ 0.2f };
	float MagicBladeSideOffset{ 0.0f };
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
	ProjectileHitBehavior HitBehavior{ ProjectileHitBehavior::Stop };
	float AreaRadius{ 0.0f };
	int MaxTargetHits{ 1 };
	int TargetHitCount{ 0 };
	std::array<int, PROJECTILE_HIT_HISTORY_MAX> HitTargetIDs{};
	int RemainingBounces{ 0 };
	int AliveLimitGroup{ PROJECTILE_INVALID_ID };
	bool UsesBezierHoming{ false };
	float BezierCurveStrength{ 0.0f };
	float BezierCurveDirection{ 1.0f };
	float BezierSpeed{ 0.0f };
	int BezierTargetID{ PROJECTILE_INVALID_ID };
	bool HasBezierSegment{ false };
	DirectX::XMFLOAT2 BezierStart{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 BezierControl{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 BezierControl2{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 BezierEnd{ 0.0f, 0.0f };
	float BezierElapsed{ 0.0f };
	float BezierDuration{ 0.0f };
	ProjectileMotionBehavior MotionBehavior{ ProjectileMotionBehavior::Linear };
	float BoomerangReturnTime{ 0.0f };
	bool IsReturningToOwner{ false };
	bool HasReachedOwner{ false };
	float OrbitRadius{ 0.0f };
	float OrbitAngularSpeed{ 0.0f };
	float OrbitPhase{ 0.0f };
	float SpinSpeed{ 0.0f };
	float RepeatHitInterval{ 0.0f };
	float RepeatHitTimer{ 0.0f };
	float MagicBladeSummonTime{ 0.0f };
	float MagicBladeReadyDelay{ 0.2f };
	float MagicBladeSideOffset{ 0.0f };
	DirectX::XMFLOAT2 MagicBladeSummonStart{ 0.0f, 0.0f };
	DirectX::XMFLOAT2 MagicBladeLaunchVelocity{ 0.0f, 0.0f };
	bool MagicBladeHasLaunched{ false };
	bool CanHitTargets{ true };
	bool UsesTrail{ false };
	int TrailTextureID{ -1 };
	float TrailEmitInterval{ 0.02f };
	float TrailEmitDistance{ 0.0f };
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
void ProjectileSystem_Update(
	float delta_time,
	const DirectX::XMFLOAT2& owner_position);
void ProjectileSystem_Draw();
void ProjectileSystem_RegisterColliders();
void ProjectileSystem_Deactivate(int projectile_id);
void ProjectileSystem_DeactivateGroup(int alive_limit_group);
int ProjectileSystem_GetActiveGroupCount(int alive_limit_group);
bool ProjectileSystem_IsActive(int projectile_id);
const cProjectile* ProjectileSystem_GetProjectile(int projectile_id);
bool ProjectileSystem_TryRegisterTargetHit(int projectile_id, int target_id);
int ProjectileSystem_GetActiveCount();
int ProjectileSystem_GetCapacity();
int ProjectileSystem_ConsumeMagicBladeLaunchEvents();

#endif // !PROJECTILE_H
