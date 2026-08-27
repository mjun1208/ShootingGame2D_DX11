#ifndef TITLE_SCENE_H
#define TITLE_SCENE_H

#include "button.h"
#include "scene.h"
#include "texture.h"

#include <memory>

namespace hal
{
	class DebugText;
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
	void SetSelectedButton(int index);
	void OpenHowTo();
	void CloseHowTo();
	void OpenCredits();
	void CloseCredits();
	void DrawMainMenu();
	void DrawHowTo();
	void DrawCredits();

	int m_LogoTextureID{ TEXTURE_INVALID_ID };
	int m_UiTextureID{ TEXTURE_INVALID_ID };
	std::unique_ptr<hal::DebugText> m_HowToHeaderText;
	std::unique_ptr<hal::DebugText> m_HowToBodyText;
	std::unique_ptr<hal::DebugText> m_CreditsHeaderText;
	std::unique_ptr<hal::DebugText> m_CreditsBodyText;
	cButton m_StartButton;
	cButton m_HowToButton;
	cButton m_CreditsButton;
	cButton m_ExitButton;
	cButton m_BackButton;
	int m_SelectedButton{ 0 };
	int m_LastMouseX{ 0 };
	int m_LastMouseY{ 0 };
	bool m_ShowHowTo{ false };
	bool m_ShowCredits{ false };
	bool m_IsTransitioning{ false };
};

#endif // !TITLE_SCENE_H
