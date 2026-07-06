#include "game_player.h"

#include "config.h"
#include "direct3d.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include "input_keyboard.h"
#include <algorithm>

#include <DirectXMath.h>
using namespace DirectX;

static int g_PlayerTextureID = TEXTURE_INVALID_ID;

static XMFLOAT2 g_PlayerPos;

static float g_PlayerSpeed = 500.0f;

static float g_PlayerWidth = 200.0f;
static float g_PlayerHeight = 100.0f;

void Game_Player_Initialize()
{
	g_PlayerPos = { SCREEN_WIDTH / 2.0f , SCREEN_HEIGHT / 2.0f };
	g_PlayerTextureID = Texture_Load(L"asset/texture/Logo.png");
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
		move.y -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_S))
	{
		move.y += 1.0f;
	}
	if (InputKeyboard_IsPress(KK_A))
	{
		move.x -= 1.0f;
	}
	if (InputKeyboard_IsPress(KK_D))
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
}

void Game_Player_Draw()
{
	Sprite_Draw(g_PlayerTextureID, g_PlayerPos.x, g_PlayerPos.y, 200, 100);
}

XMFLOAT2 Game_Player_GetBulletSpawnPosition()
{
	return { g_PlayerPos.x, g_PlayerPos.y - g_PlayerHeight / 2.0f };
}
