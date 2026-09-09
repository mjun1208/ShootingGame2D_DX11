#ifndef ENDING_SCENE_H
#define ENDING_SCENE_H

#include "button_menu_controller.h"
#include "scene.h"

#include <memory>
#include <array>

namespace hal
{
	class BitmapText;
}

enum class EndingResult
{
	Clear,
	GameOver,
};

class EndingScene final : public cScene
{
  public:
	explicit EndingScene(EndingResult result, float clear_time_seconds = 0.0f);
	~EndingScene() override;

	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

  private:
	void ActivateSelectedButton();
	void DrawFireworks();
	struct FireworkBurst
	{
		float Age{ 0.0f };
		unsigned int Sequence{ 0 };
	};
	std::array<FireworkBurst, 18> m_Fireworks{};
	int m_FireworkTextureID{ -1 };
	bool m_FireworkRendererInitialized{ false };

	const EndingResult m_Result;
	float m_ClearTimeSeconds{ 0.0f };
	std::unique_ptr<hal::BitmapText> m_HeaderText;
	std::unique_ptr<hal::BitmapText> m_TimeText;
	std::unique_ptr<hal::BitmapText> m_HintText;
	cButton m_RestartButton;
	cButton m_TitleButton;
	ButtonMenuController m_Menu;
	bool m_IsTransitioning{ false };
};

#endif // ENDING_SCENE_H
