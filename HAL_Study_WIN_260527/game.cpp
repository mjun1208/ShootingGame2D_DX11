#include "game.h"

#include "scene_manager.h"

void Game_Initialize()
{
	SceneManager_Initialize();
}

void Game_Finalize()
{
	SceneManager_Finalize();
}

void Game_Update(float delta_time)
{
	SceneManager_Update(delta_time);
}

void Game_Draw()
{
	SceneManager_Draw();
}
