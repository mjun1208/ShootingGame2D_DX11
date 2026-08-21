#include "game.h"

#include "scene_manager.h"

namespace Game
{
void Initialize()
{
	SceneManager_Initialize();
}

void Finalize()
{
	SceneManager_Finalize();
}

void Update(float delta_time)
{
	SceneManager_Update(delta_time);
}

void Draw()
{
	SceneManager_Draw();
}
}
