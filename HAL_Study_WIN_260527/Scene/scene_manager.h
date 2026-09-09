#ifndef SCENE_MANAGER_H
#define SCENE_MANAGER_H

enum class SceneID
{
	Title,
	Ingame,
	GameOver,
	Clear,
	Count,
};

bool SceneManager_Initialize(SceneID start_scene_id = SceneID::Title);
void SceneManager_Finalize();
void SceneManager_Update(float delta_time);
void SceneManager_Draw();
void SceneManager_ChangeScene(SceneID scene_id);
void SceneManager_ShowClearScene(float clear_time_seconds);
SceneID SceneManager_GetCurrentSceneID();

#endif // !SCENE_MANAGER_H
