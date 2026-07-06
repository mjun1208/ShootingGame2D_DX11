#include "collision.h"

#include <algorithm>
#include <cmath>

namespace CollisionGridConfig
{
	constexpr int CellSize = 128;
	constexpr int BucketCount = 4096;
}

static cCollisionBody g_CollisionBodies[COLLISION_BODY_MAX];
static int g_CollisionBodyCount = 0;

static cCollisionHit g_CollisionHits[COLLISION_HIT_MAX];
static int g_CollisionHitCount = 0;

struct cCollisionCellRange
{
	int MinX{ 0 };
	int MaxX{ 0 };
	int MinY{ 0 };
	int MaxY{ 0 };
};

struct cCollisionGrid
{
	int BucketHeads[CollisionGridConfig::BucketCount]{};
	int NextBody[COLLISION_BODY_MAX]{};
	int BodyCellX[COLLISION_BODY_MAX]{};
	int BodyCellY[COLLISION_BODY_MAX]{};
	float MaxRadius{ 0.0f };
};

static cCollisionGrid g_CollisionGrid;

static float Collision_ClampRadius(float radius)
{
	return std::max(radius, 0.0f);
}

static int Collision_WorldToCell(float value)
{
	return static_cast<int>(std::floor(value / static_cast<float>(CollisionGridConfig::CellSize)));
}

static int Collision_HashCell(int cell_x, int cell_y)
{
	const unsigned int x = static_cast<unsigned int>(cell_x);
	const unsigned int y = static_cast<unsigned int>(cell_y);
	const unsigned int hash = (x * 73856093u) ^ (y * 19349663u);
	return static_cast<int>(hash % CollisionGridConfig::BucketCount);
}

static cCollisionCellRange Collision_GetNearbyCellRange(const cCollisionBody& body)
{
	const int center_x = Collision_WorldToCell(body.Circle.Center.x);
	const int center_y = Collision_WorldToCell(body.Circle.Center.y);
	const int search_range =
		static_cast<int>((body.Circle.Radius + g_CollisionGrid.MaxRadius) /
			static_cast<float>(CollisionGridConfig::CellSize)) + 1;

	return {
		center_x - search_range,
		center_x + search_range,
		center_y - search_range,
		center_y + search_range,
	};
}

static bool CollisionSystem_CanHit(const cCollisionBody& a, const cCollisionBody& b)
{
	return CollisionLayer_HasAny(a.HitMask, b.Layer) || CollisionLayer_HasAny(b.HitMask, a.Layer);
}

static void CollisionSystem_AddBodyToGrid(int body_id)
{
	const cCollisionBody& body = g_CollisionBodies[body_id];
	const int cell_x = Collision_WorldToCell(body.Circle.Center.x);
	const int cell_y = Collision_WorldToCell(body.Circle.Center.y);
	const int bucket_index = Collision_HashCell(cell_x, cell_y);

	g_CollisionGrid.BodyCellX[body_id] = cell_x;
	g_CollisionGrid.BodyCellY[body_id] = cell_y;
	g_CollisionGrid.NextBody[body_id] = g_CollisionGrid.BucketHeads[bucket_index];
	g_CollisionGrid.BucketHeads[bucket_index] = body_id;
}

static void CollisionSystem_ClearGrid()
{
	g_CollisionGrid.MaxRadius = 0.0f;

	for (int i = 0; i < CollisionGridConfig::BucketCount; ++i)
	{
		g_CollisionGrid.BucketHeads[i] = COLLISION_INVALID_ID;
	}

	for (int i = 0; i < COLLISION_BODY_MAX; ++i)
	{
		g_CollisionGrid.NextBody[i] = COLLISION_INVALID_ID;
		g_CollisionGrid.BodyCellX[i] = 0;
		g_CollisionGrid.BodyCellY[i] = 0;
	}
}

static void CollisionSystem_AddHit(const cCollisionBody& a, const cCollisionBody& b)
{
	if (g_CollisionHitCount >= COLLISION_HIT_MAX)
	{
		return;
	}

	g_CollisionHits[g_CollisionHitCount] = { a, b };
	++g_CollisionHitCount;
}

static void CollisionSystem_CheckPair(int body_id_a, int body_id_b)
{
	if (body_id_a >= body_id_b)
	{
		return;
	}

	const cCollisionBody& a = g_CollisionBodies[body_id_a];
	const cCollisionBody& b = g_CollisionBodies[body_id_b];
	if (!b.IsActive || b.Layer == CollisionLayer::None)
	{
		return;
	}

	if (!CollisionSystem_CanHit(a, b))
	{
		return;
	}

	if (Collision_IsHitCircle(a.Circle, b.Circle))
	{
		CollisionSystem_AddHit(a, b);
	}
}

static void CollisionSystem_CheckNearbyBodies(int body_id)
{
	const cCollisionBody& body = g_CollisionBodies[body_id];
	if (!body.IsActive || body.Layer == CollisionLayer::None)
	{
		return;
	}

	const cCollisionCellRange range = Collision_GetNearbyCellRange(body);

	for (int grid_y = range.MinY; grid_y <= range.MaxY; ++grid_y)
	{
		for (int grid_x = range.MinX; grid_x <= range.MaxX; ++grid_x)
		{
			const int bucket_index = Collision_HashCell(grid_x, grid_y);
			for (int other_id = g_CollisionGrid.BucketHeads[bucket_index];
				other_id != COLLISION_INVALID_ID;
				other_id = g_CollisionGrid.NextBody[other_id])
			{
				if (g_CollisionGrid.BodyCellX[other_id] != grid_x ||
					g_CollisionGrid.BodyCellY[other_id] != grid_y)
				{
					continue;
				}

				CollisionSystem_CheckPair(body_id, other_id);
			}
		}
	}
}

float Collision_GetDistanceSq(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b)
{
	const float dx = a.x - b.x;
	const float dy = a.y - b.y;
	return dx * dx + dy * dy;
}

bool Collision_IsHitCircle(const cCircleCollider& a, const cCircleCollider& b)
{
	const float radius_sum = Collision_ClampRadius(a.Radius) + Collision_ClampRadius(b.Radius);
	return Collision_GetDistanceSq(a.Center, b.Center) <= radius_sum * radius_sum;
}

bool Collision_IsHitCircle(
	const DirectX::XMFLOAT2& center_a,
	float radius_a,
	const DirectX::XMFLOAT2& center_b,
	float radius_b)
{
	return Collision_IsHitCircle(
		cCircleCollider{ center_a, radius_a },
		cCircleCollider{ center_b, radius_b });
}

void CollisionSystem_Initialize()
{
	CollisionSystem_Clear();
}

void CollisionSystem_Finalize()
{
	CollisionSystem_Clear();
}

void CollisionSystem_Clear()
{
	g_CollisionBodyCount = 0;
	g_CollisionHitCount = 0;
	CollisionSystem_ClearGrid();
}

int CollisionSystem_RegisterCircle(
	int owner_id,
	CollisionLayer layer,
	CollisionLayer hit_mask,
	const DirectX::XMFLOAT2& center,
	float radius,
	bool is_active)
{
	if (g_CollisionBodyCount >= COLLISION_BODY_MAX)
	{
		return COLLISION_INVALID_ID;
	}

	const int body_id = g_CollisionBodyCount;
	const float clamped_radius = Collision_ClampRadius(radius);
	g_CollisionBodies[body_id] = {
		owner_id,
		layer,
		hit_mask,
		cCircleCollider{ center, clamped_radius },
		is_active,
	};
	g_CollisionGrid.MaxRadius = std::max(g_CollisionGrid.MaxRadius, clamped_radius);
	CollisionSystem_AddBodyToGrid(body_id);
	++g_CollisionBodyCount;

	return body_id;
}

void CollisionSystem_Update()
{
	g_CollisionHitCount = 0;

	for (int i = 0; i < g_CollisionBodyCount; ++i)
	{
		CollisionSystem_CheckNearbyBodies(i);
	}
}

int CollisionSystem_GetBodyCount()
{
	return g_CollisionBodyCount;
}

int CollisionSystem_GetHitCount()
{
	return g_CollisionHitCount;
}

const cCollisionHit* CollisionSystem_GetHit(int index)
{
	if (index < 0 || index >= g_CollisionHitCount)
	{
		return nullptr;
	}

	return &g_CollisionHits[index];
}
