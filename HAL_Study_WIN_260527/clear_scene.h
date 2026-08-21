#ifndef CLEAR_SCENE_H
#define CLEAR_SCENE_H

#include "button.h"
#include "scene.h"
#include "texture.h"

#include <memory>

namespace hal
{
	class DebugText;
}

class ClearScene final : public cScene
{
public:
	explicit ClearScene(float clear_time_seconds);
	~ClearScene() override;

	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

private:
	void ActivateSelectedButton();
	void SetSelectedButton(int index);

	float m_ClearTimeSeconds{ 0.0f };
	int m_BackgroundTextureID{ TEXTURE_INVALID_ID };
	std::unique_ptr<hal::DebugText> m_HeaderText;
	std::unique_ptr<hal::DebugText> m_TimeText;
	std::unique_ptr<hal::DebugText> m_HintText;
	cButton m_NewRunButton;
	cButton m_TitleButton;
	int m_SelectedButton{ 0 };
	int m_LastMouseX{ 0 };
	int m_LastMouseY{ 0 };
	bool m_IsTransitioning{ false };
};

#endif // CLEAR_SCENE_H
