#include "scene_manager.h"

#include "ingame_scene.h"
#include "title_scene.h"

#include <utility>

static cSceneManager g_SceneManager;

bool cSceneManager::Initialize(SceneID start_scene_id)
{
	Finalize();
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
	return g_SceneManager.Initialize(start_scene_id);
}

void SceneManager_Finalize()
{
	g_SceneManager.Finalize();
}

void SceneManager_Update(float delta_time)
{
	g_SceneManager.Update(delta_time);
}

void SceneManager_FixedUpdate()
{
	g_SceneManager.FixedUpdate();
}

void SceneManager_Draw()
{
	g_SceneManager.Draw();
}

void SceneManager_ChangeScene(SceneID scene_id)
{
	g_SceneManager.ChangeScene(scene_id);
}

SceneID SceneManager_GetCurrentSceneID()
{
	return g_SceneManager.GetCurrentSceneID();
}
