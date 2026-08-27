#ifndef SCENE_FACTORY_H
#define SCENE_FACTORY_H

#include <memory>

enum class SceneID;
class cScene;

namespace SceneFactory
{
	std::unique_ptr<cScene> Create(
		SceneID scene_id,
		float last_clear_time_seconds);
}

#endif // !SCENE_FACTORY_H
