#include "title_scene.h"

#include "config.h"
#include "input_keyboard.h"
#include "scene_manager.h"
#include "sprite.h"

bool TitleScene::Initialize()
{
	m_LogoTextureID = Texture_Load(L"asset/texture/Logo.png");
	return true;
}

void TitleScene::Finalize()
{
	Texture_Release(m_LogoTextureID);
	m_LogoTextureID = TEXTURE_INVALID_ID;
}

void TitleScene::Update(float delta_time)
{
	(void)delta_time;

	if (InputKeyboard_IsTrigger(KK_SPACE) || InputKeyboard_IsTrigger(KK_ENTER))
	{
		SceneManager_ChangeScene(SceneID::Ingame);
	}
}

void TitleScene::Draw()
{
	if (m_LogoTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	Sprite_SetFilter(kLINEAR);
	Sprite_ResetViewMatrix();
	Sprite_Draw(m_LogoTextureID, SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, 800.0f, 400.0f);
}
