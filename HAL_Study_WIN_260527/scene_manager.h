#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

#include "scene.h"

#include <memory>

enum class SceneID
{
	Title,
	Ingame,
};

class cSceneManager
{
public:
	bool Initialize(SceneID start_scene_id = SceneID::Title);
	void Finalize();
	void Update(float delta_time);
	void FixedUpdate();
	void Draw();

	void ChangeScene(SceneID scene_id);
	SceneID GetCurrentSceneID() const;

private:
	std::unique_ptr<cScene> CreateScene(SceneID scene_id);
	bool ApplySceneChange(SceneID scene_id);
	void ApplyPendingSceneChange();

	std::unique_ptr<cScene> m_CurrentScene;
	SceneID m_CurrentSceneID{ SceneID::Title };
	SceneID m_NextSceneID{ SceneID::Title };
	bool m_HasNextScene{ false };
};

bool SceneManager_Initialize(SceneID start_scene_id = SceneID::Title);
void SceneManager_Finalize();
void SceneManager_Update(float delta_time);
void SceneManager_FixedUpdate();
void SceneManager_Draw();
void SceneManager_ChangeScene(SceneID scene_id);
SceneID SceneManager_GetCurrentSceneID();

#endif // !SCENE_MANAGER_H
