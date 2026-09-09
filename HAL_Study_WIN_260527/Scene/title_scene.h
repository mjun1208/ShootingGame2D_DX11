#ifndef TITLE_SCENE_H
#define TITLE_SCENE_H

#include "button_menu_controller.h"
#include "scene.h"
#include "texture.h"

#include <array>
#include <memory>

namespace hal
{
	class BitmapText;
}

class TitleScene final : public cScene
{
  public:
	~TitleScene() override;

	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

  private:
	void UpdateMainMenu();
	void UpdateHowTo();
	void UpdateCredits();
	void ActivateSelectedButton();
	void OpenHowTo();
	void CloseHowTo();
	void OpenCredits();
	void CloseCredits();
	void DrawBackground();
	void DrawAtmosphere();
	void DrawPresentationOverlay();
	void DrawMainMenu();
	void DrawHowTo();
	void DrawCredits();

	int m_LogoTextureID{ TEXTURE_INVALID_ID };
	int m_UiTextureID{ TEXTURE_INVALID_ID };
	int m_BackgroundFarTextureID{ TEXTURE_INVALID_ID };
	int m_BackgroundMiddleTextureID{ TEXTURE_INVALID_ID };
	int m_BackgroundFrontTextureID{ TEXTURE_INVALID_ID };
	int m_WhiteTextureID{ TEXTURE_INVALID_ID };
	std::array<int, 4> m_TorchTextureIDs{
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
		TEXTURE_INVALID_ID,
	};
	std::unique_ptr<hal::BitmapText> m_HowToHeaderText;
	std::unique_ptr<hal::BitmapText> m_HowToBodyText;
	std::unique_ptr<hal::BitmapText> m_CreditsHeaderText;
	std::unique_ptr<hal::BitmapText> m_CreditsBodyText;
	cButton m_StartButton;
	cButton m_HowToButton;
	cButton m_CreditsButton;
	cButton m_ExitButton;
	cButton m_BackButton;
	cButton m_CreditsBackButton;
	ButtonMenuController m_MainMenu;
	bool m_ShowHowTo{ false };
	bool m_ShowCredits{ false };
	bool m_IsTransitioning{ false };
	float m_BackgroundTime{ 0.0f };
	float m_PresentationTime{ 0.0f };
	float m_TransitionTime{ 0.0f };
};

#endif // !TITLE_SCENE_H
