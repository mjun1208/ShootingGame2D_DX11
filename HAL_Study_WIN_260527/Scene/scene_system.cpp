#include "ending_scene.h"
#include "bgm.h"
#include "config.h"
#include "bitmap_text.h"
#include "direct3d.h"
#include "ingame_scene.h"
#include "scene_manager.h"
#include "texture.h"
#include "title_scene.h"
#include <algorithm>
#include <memory>
#include <utility>

namespace
{
	std::unique_ptr<cScene> g_CurrentScene;
	SceneID g_CurrentSceneID{ SceneID::Title };
	SceneID g_NextSceneID{ SceneID::Title };
	float g_LastClearTimeSeconds{ 0.0f };
	bool g_HasNextScene{ false };

	// BitmapText의 폰트 캐시는 소유권이 없는 포인터를 저장한다.
	// 씬 매니저가 유지되는 동안 폰트 소유자를 남겨 UI 씬을 다시 열어도 참조가 유효하도록 한다.
	std::unique_ptr<hal::BitmapText> g_UiFontKeepAlive;

	BgmTrack GetSceneBgm(SceneID scene_id)
	{
		switch (scene_id)
		{
		case SceneID::Title:
			return BgmTrack::Title;
		case SceneID::Ingame:
			return BgmTrack::Forest;
		case SceneID::GameOver:
		case SceneID::Clear:
		default:
			return BgmTrack::None;
		}
	}

	std::unique_ptr<cScene> CreateScene(SceneID scene_id)
	{
		switch (scene_id)
		{
		case SceneID::Title:
			return std::make_unique<TitleScene>();
		case SceneID::Ingame:
			return std::make_unique<IngameScene>();
		case SceneID::GameOver:
			return std::make_unique<EndingScene>(EndingResult::GameOver);
		case SceneID::Clear:
			return std::make_unique<EndingScene>(EndingResult::Clear, g_LastClearTimeSeconds);
		default:
			return nullptr;
		}
	}

	bool ApplySceneChange(SceneID scene_id)
	{
		if (g_CurrentScene)
		{
			g_CurrentScene->Finalize();
			g_CurrentScene.reset();
		}

		auto next_scene = CreateScene(scene_id);
		if (!next_scene)
		{
			return false;
		}

		if (!next_scene->Initialize())
		{
			next_scene.reset();
			return false;
		}

		g_CurrentScene = std::move(next_scene);
		g_CurrentSceneID = scene_id;
		Bgm_Play(GetSceneBgm(scene_id));
		return true;
	}

	void ApplyPendingSceneChange()
	{
		if (!g_HasNextScene)
		{
			return;
		}

		const SceneID next_scene_id = g_NextSceneID;
		g_HasNextScene = false;
		ApplySceneChange(next_scene_id);
	}
} // namespace

bool SceneManager_Initialize(SceneID start_scene_id)
{
	SceneManager_Finalize();
	Bgm_Initialize();
	g_UiFontKeepAlive = std::make_unique<hal::BitmapText>(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
	                                                      L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
	                                                      SCREEN_WIDTH, SCREEN_HEIGHT);

	if (!ApplySceneChange(start_scene_id))
	{
		SceneManager_Finalize();
		return false;
	}
	return true;
}

void SceneManager_Finalize()
{
	g_HasNextScene = false;

	if (g_CurrentScene)
	{
		g_CurrentScene->Finalize();
		g_CurrentScene.reset();
	}

	// BitmapText는 그린 뒤에도 폰트 SRV를 연결한 상태로 둔다.
	// 폰트 유지용 객체가 마지막 캐시 참조를 해제하기 전에 연결을 끊는다.
	Texture_SetTexture(TEXTURE_INVALID_ID);
	g_UiFontKeepAlive.reset();
	Bgm_Finalize();
}

void SceneManager_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	Bgm_Update(delta_time);
	if (g_CurrentScene)
	{
		g_CurrentScene->Update(delta_time);
	}

	ApplyPendingSceneChange();
}

void SceneManager_Draw()
{
	if (g_CurrentScene)
	{
		g_CurrentScene->Draw();
	}
}

void SceneManager_ChangeScene(SceneID scene_id)
{
	g_NextSceneID = scene_id;
	g_HasNextScene = true;
}

void SceneManager_ShowClearScene(float clear_time_seconds)
{
	g_LastClearTimeSeconds = std::max(clear_time_seconds, 0.0f);
	SceneManager_ChangeScene(SceneID::Clear);
}

SceneID SceneManager_GetCurrentSceneID()
{
	return g_CurrentSceneID;
}

