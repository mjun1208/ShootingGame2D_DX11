#include "application.h"

#include <algorithm>

#include "config.h"
#include "direct3d.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"

#include "input_keyboard.h"
#include "input_mouse.h"
#include "input_xinput.h"

#include "audio.h"
#include "scene_manager.h"

bool Application_Initialize(HWND hWnd)
{
	InputKeyboard_Initialize();
	InputMouse_Initialize(hWnd);
	Audio_Initialize();

	if (!Direct3D_Initialize(hWnd))
	{
		Audio_Finalize();
		return false;
	}

	Texture_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext());

	if (!Shader_Initialize(Direct3D_GetDevice(), Direct3D_GetDeviceContext()))
	{
		Texture_Finalize();
		Direct3D_Finalize();
		Audio_Finalize();
		return false;
	}

	// polygon draw 初期化
	if (!Sprite_Initialize())
	{
		Shader_Finalize();
		Texture_Finalize();
		Direct3D_Finalize();
		Audio_Finalize();
		return false;
	}

	if (!SceneManager_Initialize())
	{
		Sprite_Finalize();
		Shader_Finalize();
		Texture_Finalize();
		Direct3D_Finalize();
		Audio_Finalize();
		return false;
	}

	return true;
}

void Application_Finalize()
{
	SceneManager_Finalize();
	Audio_Finalize();
	Sprite_Finalize();
	Texture_Finalize();
	Shader_Finalize();
	Direct3D_Finalize();
}

void Application_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	InputKeyboard_Update(delta_time);
	InputMouse_Update();
	InputXInput_Update(delta_time);

	SceneManager_Update(delta_time);
	// 1秒間に150ピクセル進むように経過時間を掛ける
	// g_CocoX += 150.0f * delta_time;
}

void Application_Draw()
{
	// Direct3D_DrawBegin();
	SceneManager_Draw();
	// Direct3D_Present();
}
