#include "game_bullet.h"

#include "collision.h"
#include "projectile.h"
#include "texture.h"

#include <cmath>

static constexpr int PLAYER_OWNER_ID = 0;
static constexpr int BULLET_VARIANT_COUNT = static_cast<int>(BulletType::All);
static constexpr float ALL_TYPES_LATERAL_OFFSET = 18.0f;

struct BulletProfile
{
	float Width;
	float Height;
	float Radius;
	float Speed;
	float Damage;
	ProjectileHitBehavior HitBehavior;
	float AreaRadius;
	int MaxTargetHits;
	float TrailWidth;
	float TrailLength;
	float TrailOffset;
	DirectX::XMFLOAT4 TrailColor;
};

static constexpr BulletProfile BULLET_PROFILES[BULLET_VARIANT_COUNT] =
{
	// Fireball: slower projectile that damages every enemy near its impact.
	{ 52.0f, 64.0f, 14.0f, 820.0f, 8.0f,
		ProjectileHitBehavior::Area, 180.0f, 1,
		44.0f, 108.0f, 46.0f, { 1.00f, 0.35f, 0.06f, 0.58f } },
	// Lightning: fast direct hit followed by guaranteed chain lightning.
	{ 36.0f, 76.0f, 12.0f, 1050.0f, 7.0f,
		ProjectileHitBehavior::ChainLightning, 0.0f, 1,
		30.0f, 138.0f, 58.0f, { 0.12f, 0.72f, 1.00f, 0.58f } },
	// Piercing: travels through up to six distinct enemies.
	{ 44.0f, 46.0f, 13.0f, 900.0f, 8.0f,
		ProjectileHitBehavior::Pierce, 0.0f, 6,
		38.0f, 108.0f, 44.0f, { 0.68f, 0.22f, 1.00f, 0.56f } },
};

static constexpr const wchar_t* BULLET_TEXTURE_PATHS[BULLET_VARIANT_COUNT] =
{
	L"asset/texture/projectile/fireball.png",
	L"asset/texture/projectile/storm_arrow.png",
	L"asset/texture/projectile/void_orb.png",
};

static int g_BulletTextureIDs[BULLET_VARIANT_COUNT] =
{
	TEXTURE_INVALID_ID,
	TEXTURE_INVALID_ID,
	TEXTURE_INVALID_ID,
};
static int g_TrailTextureID = TEXTURE_INVALID_ID;
static BulletType g_BulletType = BulletType::Fireball;

static bool IsValidBulletType(BulletType type)
{
	const int index = static_cast<int>(type);
	return index >= 0 && index <= static_cast<int>(BulletType::All);
}

static void FireSingleType(
	BulletType type,
	const DirectX::XMFLOAT2& spawn_position,
	const DirectX::XMFLOAT2& normalized_direction)
{
	const int bullet_type_index = static_cast<int>(type);
	if (bullet_type_index < 0 || bullet_type_index >= BULLET_VARIANT_COUNT)
	{
		return;
	}

	const BulletProfile& profile = BULLET_PROFILES[bullet_type_index];
	cProjectileDesc desc{};
	desc.Position = spawn_position;
	desc.Velocity = {
		normalized_direction.x * profile.Speed,
		normalized_direction.y * profile.Speed,
	};
	desc.Radius = profile.Radius;
	desc.Width = profile.Width;
	desc.Height = profile.Height;
	desc.Rotation = std::atan2(normalized_direction.x, -normalized_direction.y);
	desc.Damage = profile.Damage;
	desc.LifeTime = 2.6f;
	desc.TextureID = g_BulletTextureIDs[bullet_type_index];
	desc.OwnerID = PLAYER_OWNER_ID;
	desc.Layer = CollisionLayer::PlayerBullet;
	desc.HitMask = CollisionLayer::Enemy;
	desc.HitBehavior = profile.HitBehavior;
	desc.AreaRadius = profile.AreaRadius;
	desc.MaxTargetHits = profile.MaxTargetHits;
	desc.UsesTrail = true;
	desc.TrailTextureID = g_TrailTextureID;
	desc.TrailEmitInterval = 0.01f;
	desc.TrailWidth = profile.TrailWidth;
	desc.TrailLength = profile.TrailLength;
	desc.TrailOffset = profile.TrailOffset;
	desc.TrailLifeTime = 0.13f;
	desc.TrailStartScale = 1.0f;
	desc.TrailEndScale = 0.28f;
	desc.TrailColor = profile.TrailColor;

	ProjectileSystem_Fire(desc);
}

namespace GameBullet
{
void Initialize()
{
	ProjectileSystem_Initialize();
	for (int i = 0; i < BULLET_VARIANT_COUNT; ++i)
	{
		g_BulletTextureIDs[i] = Texture_Load(BULLET_TEXTURE_PATHS[i]);
	}
	g_TrailTextureID = Texture_Load(L"asset/texture/trail_glow.png");
	g_BulletType = BulletType::Fireball;
}

void Finalize()
{
	ProjectileSystem_Finalize();
	Texture_Release(g_TrailTextureID);
	g_TrailTextureID = TEXTURE_INVALID_ID;
	for (int i = 0; i < BULLET_VARIANT_COUNT; ++i)
	{
		Texture_Release(g_BulletTextureIDs[i]);
		g_BulletTextureIDs[i] = TEXTURE_INVALID_ID;
	}
}

void SetType(BulletType type)
{
	if (IsValidBulletType(type))
	{
		g_BulletType = type;
	}
}

BulletType GetType()
{
	return g_BulletType;
}

void Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& direction)
{
	const float direction_length_sq = direction.x * direction.x + direction.y * direction.y;
	if (direction_length_sq <= 0.0001f)
	{
		return;
	}

	const float inv_direction_length = 1.0f / std::sqrt(direction_length_sq);
	const DirectX::XMFLOAT2 normalized_direction = {
		direction.x * inv_direction_length,
		direction.y * inv_direction_length,
	};
	if (g_BulletType == BulletType::All)
	{
		constexpr BulletType ALL_TYPES[] = {
			BulletType::Fireball,
			BulletType::Lightning,
			BulletType::Piercing,
		};
		const DirectX::XMFLOAT2 perpendicular = {
			-normalized_direction.y,
			normalized_direction.x,
		};
		for (int i = 0; i < BULLET_VARIANT_COUNT; ++i)
		{
			const float lateral_offset =
				(static_cast<float>(i) - 1.0f) * ALL_TYPES_LATERAL_OFFSET;
			const DirectX::XMFLOAT2 offset_position = {
				spawn_position.x + perpendicular.x * lateral_offset,
				spawn_position.y + perpendicular.y * lateral_offset,
			};
			FireSingleType(ALL_TYPES[i], offset_position, normalized_direction);
		}
		return;
	}

	FireSingleType(g_BulletType, spawn_position, normalized_direction);
}

void Update(float delta_time)
{
	ProjectileSystem_Update(delta_time);
}

void Clear()
{
	ProjectileSystem_Clear();
}

void Draw()
{
	ProjectileSystem_Draw();
}

void RegisterColliders()
{
	ProjectileSystem_RegisterColliders();
}
}
