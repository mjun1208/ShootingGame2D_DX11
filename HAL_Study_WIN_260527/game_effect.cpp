#include "game_effect.h"

#include "effect.h"
#include "texture.h"

static constexpr int EFFECT_MAX = 256;
static constexpr int EXPLOSION_FRAME_WIDTH = 128;
static constexpr int EXPLOSION_FRAME_HEIGHT = 128;
static constexpr int EXPLOSION_FRAME_COUNT = 8;
static constexpr float EXPLOSION_FRAME_TIME = 0.045f;
static constexpr float EXPLOSION_DRAW_SIZE = 150.0f;

static cEffect g_Effects[EFFECT_MAX];
static int g_ExplosionTextureID = TEXTURE_INVALID_ID;

void Game_Effect_Initialize()
{
	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		g_Effects[i] = cEffect{};
	}

	g_ExplosionTextureID = Texture_Load(L"asset/texture/effect_explosion.png");
}

void Game_Effect_Finalize()
{
	Texture_Release(g_ExplosionTextureID);
	g_ExplosionTextureID = TEXTURE_INVALID_ID;
}

void Game_Effect_Update(float delta_time)
{
	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		g_Effects[i].Update(delta_time);
	}
}

void Game_Effect_Draw()
{
	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		g_Effects[i].Draw();
	}
}

void Game_Effect_PlayExplosion(const DirectX::XMFLOAT2& position)
{
	for (int i = 0; i < EFFECT_MAX; ++i)
	{
		if (g_Effects[i].IsActive())
		{
			continue;
		}

		cEffectDesc desc{};
		desc.Position = position;
		desc.TextureID = g_ExplosionTextureID;
		desc.FrameWidth = EXPLOSION_FRAME_WIDTH;
		desc.FrameHeight = EXPLOSION_FRAME_HEIGHT;
		desc.FrameCount = EXPLOSION_FRAME_COUNT;
		desc.FrameTime = EXPLOSION_FRAME_TIME;
		desc.DrawWidth = EXPLOSION_DRAW_SIZE;
		desc.DrawHeight = EXPLOSION_DRAW_SIZE;
		g_Effects[i].Play(desc);
		break;
	}
}
