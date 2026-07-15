#include "game_enemy.h"

#include "collision.h"
#include "config.h"
#include "enemy.h"
#include "game_effect.h"
#include "game_player.h"
#include "projectile.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <vector>

static constexpr int ENEMY_MAX = 128;
static constexpr float ENEMY_SPAWN_INTERVAL = 0.5f;
static constexpr float ENEMY_SPAWN_EDGE_PADDING = 64.0f;
static constexpr float ENEMY_SPAWN_OUTSIDE_OFFSET = 80.0f;
static constexpr float ENEMY_BASE_SPEED = 120.0f;
static constexpr float ENEMY_SPEED_STEP = 18.0f;

static cEnemy g_Enemies[ENEMY_MAX];
static int g_EnemyTextureID = TEXTURE_INVALID_ID;
static int g_DissolveNoiseTextureID = TEXTURE_INVALID_ID;
static float g_EnemySpawnTimer = 0.0f;
static int g_EnemySpawnCount = 0;

static bool Game_Enemy_IsValidID(int enemy_id)
{
	return enemy_id >= 0 && enemy_id < ENEMY_MAX;
}

static float Game_Enemy_GetSpawnT(int spawn_count)
{
	const int lane_seed = (spawn_count * 157) % 1000;
	return lane_seed / 999.0f;
}

static void Game_Enemy_Spawn()
{
	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		if (g_Enemies[i].IsActive())
		{
			continue;
		}

		const float screen_width = static_cast<float>(SCREEN_WIDTH);
		const float screen_height = static_cast<float>(SCREEN_HEIGHT);
		const float side_t = Game_Enemy_GetSpawnT(g_EnemySpawnCount);
		const float spawn_width = screen_width - ENEMY_SPAWN_EDGE_PADDING * 2.0f;
		const float spawn_height = screen_height - ENEMY_SPAWN_EDGE_PADDING * 2.0f;
		const float outside_x = cEnemy::WIDTH * 0.5f + ENEMY_SPAWN_OUTSIDE_OFFSET;
		const float outside_y = cEnemy::HEIGHT * 0.5f + ENEMY_SPAWN_OUTSIDE_OFFSET;
		DirectX::XMFLOAT2 spawn_position{};

		switch (g_EnemySpawnCount % 4)
		{
		case 0:
			spawn_position = { ENEMY_SPAWN_EDGE_PADDING + spawn_width * side_t, -outside_y };
			break;
		case 1:
			spawn_position = { screen_width + outside_x, ENEMY_SPAWN_EDGE_PADDING + spawn_height * side_t };
			break;
		case 2:
			spawn_position = { ENEMY_SPAWN_EDGE_PADDING + spawn_width * (1.0f - side_t), screen_height + outside_y };
			break;
		default:
			spawn_position = { -outside_x, ENEMY_SPAWN_EDGE_PADDING + spawn_height * (1.0f - side_t) };
			break;
		}

		const float speed = ENEMY_BASE_SPEED + static_cast<float>(g_EnemySpawnCount % 5) * ENEMY_SPEED_STEP;

		g_Enemies[i].Spawn(spawn_position, speed);
		++g_EnemySpawnCount;
		break;
	}
}

static bool Game_Enemy_TryGetEnemyAndProjectile(
	const cCollisionHit& hit,
	int& enemy_id,
	int& projectile_id)
{
	if (hit.BodyA.Layer == CollisionLayer::Enemy &&
		hit.BodyB.Layer == CollisionLayer::PlayerBullet)
	{
		enemy_id = hit.BodyA.OwnerID;
		projectile_id = hit.BodyB.OwnerID;
		return true;
	}

	if (hit.BodyA.Layer == CollisionLayer::PlayerBullet &&
		hit.BodyB.Layer == CollisionLayer::Enemy)
	{
		enemy_id = hit.BodyB.OwnerID;
		projectile_id = hit.BodyA.OwnerID;
		return true;
	}

	return false;
}

void Game_Enemy_Initialize()
{
	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		g_Enemies[i] = cEnemy{};
	}

	g_EnemyTextureID = Texture_Load(L"asset/texture/enemy.png");
	g_DissolveNoiseTextureID = Texture_Load(L"asset/texture/dissolve_noise.png");
	g_EnemySpawnTimer = 0.3f;
	g_EnemySpawnCount = 0;
}

void Game_Enemy_Finalize()
{
	Texture_Release(g_EnemyTextureID);
	Texture_Release(g_DissolveNoiseTextureID);
	g_EnemyTextureID = TEXTURE_INVALID_ID;
	g_DissolveNoiseTextureID = TEXTURE_INVALID_ID;
}

void Game_Enemy_Update(float delta_time)
{
	const DirectX::XMFLOAT2 player_position = Game_Player_GetPosition();

	g_EnemySpawnTimer -= delta_time;
	if (g_EnemySpawnTimer <= 0.0f)
	{
		Game_Enemy_Spawn();
		g_EnemySpawnTimer += ENEMY_SPAWN_INTERVAL;
	}

	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		g_Enemies[i].Update(delta_time, player_position);
	}
}

void Game_Enemy_Draw()
{
	static std::vector<SpriteInstance> alive_instances;
	if (alive_instances.capacity() < ENEMY_MAX)
	{
		alive_instances.reserve(ENEMY_MAX);
	}
	alive_instances.clear();

	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		if (g_Enemies[i].IsAlive())
		{
			alive_instances.push_back({
				g_Enemies[i].GetPosition(),
				{ cEnemy::WIDTH, cEnemy::HEIGHT },
				0.0f,
				{ 1.0f, 1.0f, 1.0f, 1.0f },
			});
		}
		else if (g_Enemies[i].IsActive())
		{
			g_Enemies[i].Draw(g_EnemyTextureID, g_DissolveNoiseTextureID);
		}
	}

	SpriteInstanced_Draw(
		g_EnemyTextureID, alive_instances.data(), static_cast<int>(alive_instances.size()));
}

void Game_Enemy_RegisterColliders()
{
	for (int i = 0; i < ENEMY_MAX; ++i)
	{
		g_Enemies[i].RegisterCollider(i);
	}
}

void Game_Enemy_HandleCollisionHits()
{
	for (int i = 0; i < CollisionSystem_GetHitCount(); ++i)
	{
		const cCollisionHit* hit = CollisionSystem_GetHit(i);
		if (!hit)
		{
			continue;
		}

		int enemy_id = -1;
		int projectile_id = PROJECTILE_INVALID_ID;
		if (!Game_Enemy_TryGetEnemyAndProjectile(*hit, enemy_id, projectile_id))
		{
			continue;
		}

		if (!Game_Enemy_IsValidID(enemy_id) || !g_Enemies[enemy_id].IsAlive())
		{
			continue;
		}

		const cProjectile* projectile = ProjectileSystem_GetProjectile(projectile_id);
		if (!projectile)
		{
			continue;
		}

		const DirectX::XMFLOAT2 enemy_position = g_Enemies[enemy_id].GetPosition();
		g_Enemies[enemy_id].ApplyDamage(projectile->Damage);
		if (!g_Enemies[enemy_id].IsAlive())
		{
			Game_Effect_PlayExplosion(enemy_position);
		}

		ProjectileSystem_Deactivate(projectile_id);
	}
}

void Game_Enemy_Deactivate(int enemy_id)
{
	if (!Game_Enemy_IsValidID(enemy_id))
	{
		return;
	}

	g_Enemies[enemy_id].Deactivate();
}

bool Game_Enemy_IsActive(int enemy_id)
{
	return Game_Enemy_IsValidID(enemy_id) && g_Enemies[enemy_id].IsActive();
}
