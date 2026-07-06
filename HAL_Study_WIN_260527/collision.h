#ifndef COLLISION_H
#define COLLISION_H

#include <DirectXMath.h>

constexpr int COLLISION_INVALID_ID = -1;
constexpr int COLLISION_BODY_MAX = 16384;
constexpr int COLLISION_HIT_MAX = 8192;

enum class CollisionLayer : unsigned int
{
	None = 0u,
	Player = 1u << 0,
	PlayerBullet = 1u << 1,
	Enemy = 1u << 2,
	EnemyBullet = 1u << 3,
	Item = 1u << 4,
	All = 0xffffffffu,
};

constexpr unsigned int CollisionLayer_ToUInt(CollisionLayer layer)
{
	return static_cast<unsigned int>(layer);
}

constexpr CollisionLayer operator|(CollisionLayer a, CollisionLayer b)
{
	return static_cast<CollisionLayer>(CollisionLayer_ToUInt(a) | CollisionLayer_ToUInt(b));
}

constexpr CollisionLayer operator&(CollisionLayer a, CollisionLayer b)
{
	return static_cast<CollisionLayer>(CollisionLayer_ToUInt(a) & CollisionLayer_ToUInt(b));
}

inline CollisionLayer& operator|=(CollisionLayer& a, CollisionLayer b)
{
	a = a | b;
	return a;
}

constexpr bool CollisionLayer_HasAny(CollisionLayer a, CollisionLayer b)
{
	return CollisionLayer_ToUInt(a & b) != 0u;
}

struct cCircleCollider
{
	DirectX::XMFLOAT2 Center{ 0.0f, 0.0f };
	float Radius{ 0.0f };
};

struct cCollisionBody
{
	int OwnerID{ COLLISION_INVALID_ID };
	CollisionLayer Layer{ CollisionLayer::None };
	CollisionLayer HitMask{ CollisionLayer::None };
	cCircleCollider Circle{};
	bool IsActive{ true };
};

struct cCollisionHit
{
	cCollisionBody BodyA{};
	cCollisionBody BodyB{};
};

float Collision_GetDistanceSq(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b);
bool Collision_IsHitCircle(const cCircleCollider& a, const cCircleCollider& b);
bool Collision_IsHitCircle(
	const DirectX::XMFLOAT2& center_a,
	float radius_a,
	const DirectX::XMFLOAT2& center_b,
	float radius_b);

void CollisionSystem_Initialize();
void CollisionSystem_Finalize();
void CollisionSystem_Clear();
int CollisionSystem_RegisterCircle(
	int owner_id,
	CollisionLayer layer,
	CollisionLayer hit_mask,
	const DirectX::XMFLOAT2& center,
	float radius,
	bool is_active = true);
void CollisionSystem_Update();
int CollisionSystem_GetBodyCount();
int CollisionSystem_GetHitCount();
const cCollisionHit* CollisionSystem_GetHit(int index);

#endif // !COLLISION_H
