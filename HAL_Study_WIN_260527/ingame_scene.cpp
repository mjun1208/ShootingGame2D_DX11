#include "ingame_scene.h"

#include "config.h"
#include "collision.h"
#include "game_bullet.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "game_player.h"
#include "input_keyboard.h"
#include "sprite.h"

bool IngameScene::Initialize()
{
	m_Camera.SetScreenSize((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
	m_Camera.SetPosition(SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f);

	Game_Player_Initialize();
	Game_Bullet_Initialize();
	Game_Enemy_Initialize();
	Game_Effect_Initialize();
	CollisionSystem_Initialize();

	return true;
}

void IngameScene::Finalize()
{
	CollisionSystem_Finalize();
	Game_Effect_Finalize();
	Game_Enemy_Finalize();
	Game_Bullet_Finalize();
	Game_Player_Finalize();
}

void IngameScene::Update(float delta_time)
{
	Game_Player_Update(delta_time);

	if (InputKeyboard_IsRepeat(KK_SPACE, 0.15f, 0.08f))
	{
		Game_Bullet_Fire(Game_Player_GetBulletSpawnPosition());
	}

	Game_Bullet_Update(delta_time);
	Game_Enemy_Update(delta_time);
	Game_Effect_Update(delta_time);

	CollisionSystem_Clear();
	Game_Bullet_RegisterColliders();
	Game_Enemy_RegisterColliders();
	CollisionSystem_Update();
	Game_Enemy_HandleCollisionHits();
}

void IngameScene::Draw()
{
	Sprite_SetFilter(kPOINT);

	Sprite_SetViewMatrix(m_Camera.GetViewMatrix());
	Game_Player_Draw();
	Game_Enemy_Draw();
	Game_Bullet_Draw();
	Game_Effect_Draw();
	Sprite_ResetViewMatrix();
}
