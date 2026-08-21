#include "scene_manager.h"

#include "clear_scene.h"
#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "game_over_scene.h"
#include "ingame_scene.h"
#include "texture.h"
#include "title_scene.h"

#include <algorithm>
#include <utility>

namespace
{
	// DebugText's font cache stores non-owning raw pointers. Keeping one owner
	// alive for the scene manager's full lifetime makes repeated UI scenes safe.
	std::unique_ptr<hal::DebugText> g_UiFontKeepAlive;
}

bool cSceneManager::Initialize(SceneID start_scene_id)
{
	Finalize();
	g_UiFontKeepAlive = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(),
		Direct3D_GetDeviceContext(),
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
		SCREEN_WIDTH,
		SCREEN_HEIGHT);

	return ApplySceneChange(start_scene_id);
}

void cSceneManager::Finalize()
{
	m_HasNextScene = false;

	if (m_CurrentScene)
	{
		m_CurrentScene->Finalize();
		m_CurrentScene.reset();
	}

	// DebugText leaves its font SRV bound after drawing. Unbind it before the
	// keep-alive releases the last cache reference.
	Texture_SetTexture(TEXTURE_INVALID_ID);
	g_UiFontKeepAlive.reset();
}

void cSceneManager::Update(float delta_time)
{
	if (m_CurrentScene)
	{
		m_CurrentScene->Update(delta_time);
	}

	ApplyPendingSceneChange();
}

void cSceneManager::FixedUpdate()
{
	if (m_CurrentScene)
	{
		m_CurrentScene->FixedUpdate();
	}

	ApplyPendingSceneChange();
}

void cSceneManager::Draw()
{
	if (m_CurrentScene)
	{
		m_CurrentScene->Draw();
	}
}

void cSceneManager::ChangeScene(SceneID scene_id)
{
	m_NextSceneID = scene_id;
	m_HasNextScene = true;
}

void cSceneManager::ShowClearScene(float clear_time_seconds)
{
	m_LastClearTimeSeconds = std::max(clear_time_seconds, 0.0f);
	ChangeScene(SceneID::Clear);
}

SceneID cSceneManager::GetCurrentSceneID() const
{
	return m_CurrentSceneID;
}

std::unique_ptr<cScene> cSceneManager::CreateScene(SceneID scene_id)
{
	switch (scene_id)
	{
	case SceneID::Title:
		return std::make_unique<TitleScene>();
	case SceneID::Ingame:
		return std::make_unique<IngameScene>();
	case SceneID::GameOver:
		return std::make_unique<GameOverScene>();
	case SceneID::Clear:
		return std::make_unique<ClearScene>(m_LastClearTimeSeconds);
	default:
		return nullptr;
	}
}

bool cSceneManager::ApplySceneChange(SceneID scene_id)
{
	if (m_CurrentScene)
	{
		m_CurrentScene->Finalize();
		m_CurrentScene.reset();
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

	m_CurrentScene = std::move(next_scene);
	m_CurrentSceneID = scene_id;
	return true;
}

void cSceneManager::ApplyPendingSceneChange()
{
	if (!m_HasNextScene)
	{
		return;
	}

	const SceneID next_scene_id = m_NextSceneID;
	m_HasNextScene = false;
	ApplySceneChange(next_scene_id);
}

bool SceneManager_Initialize(SceneID start_scene_id)
{
	return cSceneManager::GetInstance().Initialize(start_scene_id);
}

void SceneManager_Finalize()
{
	cSceneManager::GetInstance().Finalize();
}

void SceneManager_Update(float delta_time)
{
	cSceneManager::GetInstance().Update(delta_time);
}

void SceneManager_FixedUpdate()
{
	cSceneManager::GetInstance().FixedUpdate();
}

void SceneManager_Draw()
{
	cSceneManager::GetInstance().Draw();
}

void SceneManager_ChangeScene(SceneID scene_id)
{
	cSceneManager::GetInstance().ChangeScene(scene_id);
}

void SceneManager_ShowClearScene(float clear_time_seconds)
{
	cSceneManager::GetInstance().ShowClearScene(clear_time_seconds);
}

SceneID SceneManager_GetCurrentSceneID()
{
	return cSceneManager::GetInstance().GetCurrentSceneID();
}
