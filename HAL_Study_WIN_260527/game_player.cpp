#include "game_player.h"

#include "config.h"
#include "direct3d.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include <algorithm>
#include <cmath>

#include <DirectXMath.h>
#include "audio.h"
using namespace DirectX;

static int g_PlayerTextureID = TEXTURE_INVALID_ID;

static XMFLOAT2 g_PlayerPos;
static XMFLOAT2 g_PlayerAimDirection = { 0.0f, -1.0f };

static float g_PlayerSpeed = 500.0f;

static float g_PlayerWidth = 200.0f;
static float g_PlayerHeight = 100.0f;
static constexpr float PLAYER_BULLET_SPAWN_OFFSET = 72.0f;
static constexpr float PLAYER_AIM_DEAD_ZONE_SQ = 16.0f;

static int g_SoundId = 0;

void Game_Player_Initialize()
{
	g_PlayerPos = { SCREEN_WIDTH / 2.0f , SCREEN_HEIGHT / 2.0f };
	g_PlayerAimDirection = { 0.0f, -1.0f };
	g_PlayerTextureID = Texture_Load(L"asset/texture/Logo.png");

	g_SoundId = LoadAudio("asset/sound/test.wav");
}

void Game_Player_Finalize()
{
	Texture_Release(g_PlayerTextureID);
}

void Game_Player_Update(float delta_time)
{
	XMFLOAT2 move = { 0.0f, 0.0f };

	if (InputKeyboard_IsPress(KK_W))
	{
		PlayAudio(g_SoundId);
		move.y -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_UP))
	{
		move.y -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_S))
	{
		move.y += 1.0f;
	}
	if (InputKeyboard_IsPress(KK_DOWN))
	{
		move.y += 1.0f;
	}
	if (InputKeyboard_IsPress(KK_A))
	{
		move.x -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_LEFT))
	{
		move.x -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_D))
	{
		move.x += 1.0f;
	}
	if (InputKeyboard_IsPress(KK_RIGHT))
	{
		move.x += 1.0f;
	}

	if (move.x != 0.0f || move.y != 0.0f)
	{
		XMStoreFloat2(&move, XMVector2Normalize(XMLoadFloat2(&move)));

		g_PlayerPos.x += move.x * g_PlayerSpeed * delta_time;
		g_PlayerPos.y += move.y * g_PlayerSpeed * delta_time;
	}

	g_PlayerPos.x = std::clamp(g_PlayerPos.x, g_PlayerWidth / 2.0f, (float)SCREEN_WIDTH - g_PlayerWidth / 2.0f);
	g_PlayerPos.y = std::clamp(g_PlayerPos.y, g_PlayerHeight / 2.0f, (float)SCREEN_HEIGHT - g_PlayerHeight / 2.0f);

	const XMFLOAT2 mouse_position = {
		static_cast<float>(InputMouse_GetX()),
		static_cast<float>(InputMouse_GetY()),
	};
	const XMFLOAT2 mouse_aim = {
		mouse_position.x - g_PlayerPos.x,
		mouse_position.y - g_PlayerPos.y,
	};
	const float aim_length_sq = mouse_aim.x * mouse_aim.x + mouse_aim.y * mouse_aim.y;
	if (aim_length_sq > PLAYER_AIM_DEAD_ZONE_SQ)
	{
		const float inv_aim_length = 1.0f / std::sqrt(aim_length_sq);
		g_PlayerAimDirection = {
			mouse_aim.x * inv_aim_length,
			mouse_aim.y * inv_aim_length,
		};
	}
	else if (move.x != 0.0f || move.y != 0.0f)
	{
		g_PlayerAimDirection = move;
	}
}

void Game_Player_Draw()
{
	Sprite_Draw(g_PlayerTextureID, g_PlayerPos.x, g_PlayerPos.y, 200, 100);
}

XMFLOAT2 Game_Player_GetBulletSpawnPosition()
{
	return {
		g_PlayerPos.x + g_PlayerAimDirection.x * PLAYER_BULLET_SPAWN_OFFSET,
		g_PlayerPos.y + g_PlayerAimDirection.y * PLAYER_BULLET_SPAWN_OFFSET,
	};
}

XMFLOAT2 Game_Player_GetAimDirection()
{
	return g_PlayerAimDirection;
}

XMFLOAT2 Game_Player_GetPosition()
{
	return g_PlayerPos;
}
