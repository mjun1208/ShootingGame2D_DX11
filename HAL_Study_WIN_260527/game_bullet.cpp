#include "game_bullet.h"

#include "collision.h"
#include "projectile.h"
#include "texture.h"

static constexpr int PLAYER_OWNER_ID = 0;
static constexpr float PLAYER_BULLET_WIDTH = 32.0f;
static constexpr float PLAYER_BULLET_HEIGHT = 78.0f;
static constexpr float PLAYER_BULLET_RADIUS = 14.0f;
static constexpr float PLAYER_BULLET_SPEED = 900.0f;
static constexpr float PLAYER_BULLET_DAMAGE = 1.0f;

static int g_BulletTextureID = TEXTURE_INVALID_ID;
static int g_TrailTextureID = TEXTURE_INVALID_ID;

void Game_Bullet_Initialize()
{
	ProjectileSystem_Initialize();
	g_BulletTextureID = Texture_Load(L"asset/texture/bullet.png");
	g_TrailTextureID = Texture_Load(L"asset/texture/trail_glow.png");
}

void Game_Bullet_Finalize()
{
	ProjectileSystem_Finalize();
	Texture_Release(g_TrailTextureID);
	Texture_Release(g_BulletTextureID);
	g_TrailTextureID = TEXTURE_INVALID_ID;
	g_BulletTextureID = TEXTURE_INVALID_ID;
}

void Game_Bullet_Fire(const DirectX::XMFLOAT2& spawn_position)
{
	auto bullet_position = spawn_position;
	bullet_position.y -= PLAYER_BULLET_HEIGHT / 2.0f;

	cProjectileDesc desc{};
	desc.Position = bullet_position;
	desc.Velocity = { 0.0f, -PLAYER_BULLET_SPEED };
	desc.Radius = PLAYER_BULLET_RADIUS;
	desc.Width = PLAYER_BULLET_WIDTH;
	desc.Height = PLAYER_BULLET_HEIGHT;
	desc.Damage = PLAYER_BULLET_DAMAGE;
	desc.TextureID = g_BulletTextureID;
	desc.OwnerID = PLAYER_OWNER_ID;
	desc.Layer = CollisionLayer::PlayerBullet;
	desc.HitMask = CollisionLayer::Enemy;
	desc.UsesTrail = true;
	desc.TrailTextureID = g_TrailTextureID;
	desc.TrailEmitInterval = 0.01f;
	desc.TrailWidth = 34.0f;
	desc.TrailLength = 140.0f;
	desc.TrailOffset = 58.0f;
	desc.TrailLifeTime = 0.13f;
	desc.TrailStartScale = 1.0f;
	desc.TrailEndScale = 0.28f;
	desc.TrailColor = { 0.45f, 0.9f, 1.0f, 0.55f };

	ProjectileSystem_Fire(desc);
}

void Game_Bullet_Update(float delta_time)
{
	ProjectileSystem_Update(delta_time);
}

void Game_Bullet_Draw()
{
	ProjectileSystem_Draw();
}

void Game_Bullet_RegisterColliders()
{
	ProjectileSystem_RegisterColliders();
}
