#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "scene.h"
#include "singleton.h"

#include <memory>

enum class SceneID
{
	Title,
	Ingame,
	GameOver,
	Clear,
	Count,
};

class cSceneManager : public cSingleton<cSceneManager>
{
	friend class cSingleton<cSceneManager>;

public:
	bool Initialize(SceneID start_scene_id = SceneID::Title);
	void Finalize();
	void Update(float delta_time);
	void FixedUpdate();
	void Draw();

	void ChangeScene(SceneID scene_id);
	void ShowClearScene(float clear_time_seconds);
	SceneID GetCurrentSceneID() const;

private:
	cSceneManager() = default;
	~cSceneManager() = default;

	bool ApplySceneChange(SceneID scene_id);
	void ApplyPendingSceneChange();

	std::unique_ptr<cScene> m_CurrentScene;
	SceneID m_CurrentSceneID{ SceneID::Title };
	SceneID m_NextSceneID{ SceneID::Title };
	float m_LastClearTimeSeconds{ 0.0f };
	bool m_HasNextScene{ false };
};

bool SceneManager_Initialize(SceneID start_scene_id = SceneID::Title);
void SceneManager_Finalize();
void SceneManager_Update(float delta_time);
void SceneManager_FixedUpdate();
void SceneManager_Draw();
void SceneManager_ChangeScene(SceneID scene_id);
void SceneManager_ShowClearScene(float clear_time_seconds);
SceneID SceneManager_GetCurrentSceneID();

#endif // !SCENE_MANAGER_H
