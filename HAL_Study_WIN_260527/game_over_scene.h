#ifndef GAME_OVER_SCENE_H
#define GAME_OVER_SCENE_H

#include "button.h"
#include "scene.h"
#include "texture.h"

#include <memory>

namespace hal
{
	class DebugText;
}

class GameOverScene final : public cScene
{
public:
	~GameOverScene() override;

	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

private:
	void ActivateSelectedButton();
	void SetSelectedButton(int index);

	int m_BackgroundTextureID{ TEXTURE_INVALID_ID };
	std::unique_ptr<hal::DebugText> m_HeaderText;
	std::unique_ptr<hal::DebugText> m_SubtitleText;
	std::unique_ptr<hal::DebugText> m_HintText;
	cButton m_RetryButton;
	cButton m_TitleButton;
	float m_ElapsedTime{ 0.0f };
	int m_SelectedButton{ 0 };
	int m_LastMouseX{ 0 };
	int m_LastMouseY{ 0 };
	bool m_IsTransitioning{ false };
};

#endif // GAME_OVER_SCENE_H
