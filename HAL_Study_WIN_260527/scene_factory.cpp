#include "scene_factory.h"

#include "clear_scene.h"
#include "debug_text.h"
#include "game_over_scene.h"
#include "ingame_scene.h"
#include "scene_manager.h"
#include "title_scene.h"

#include <array>

namespace
{
	using SceneCreator = std::unique_ptr<cScene>(*)(float);

	std::unique_ptr<cScene> CreateTitleScene(float)
	{
		return std::make_unique<TitleScene>();
	}

	std::unique_ptr<cScene> CreateIngameScene(float)
	{
		return std::make_unique<IngameScene>();
	}

	std::unique_ptr<cScene> CreateGameOverScene(float)
	{
		return std::make_unique<GameOverScene>();
	}

	std::unique_ptr<cScene> CreateClearScene(float clear_time_seconds)
	{
		return std::make_unique<ClearScene>(clear_time_seconds);
	}

	constexpr std::array<
		SceneCreator,
		static_cast<std::size_t>(SceneID::Count)> SCENE_CREATORS = {
		CreateTitleScene,
		CreateIngameScene,
		CreateGameOverScene,
		CreateClearScene,
	};
}

namespace SceneFactory
{
std::unique_ptr<cScene> Create(
	SceneID scene_id,
	float last_clear_time_seconds)
{
	const int scene_index = static_cast<int>(scene_id);
	if (scene_index < 0 ||
		scene_index >= static_cast<int>(SCENE_CREATORS.size()))
	{
		return nullptr;
	}
	return SCENE_CREATORS[static_cast<std::size_t>(scene_index)](
		last_clear_time_seconds);
}
}
