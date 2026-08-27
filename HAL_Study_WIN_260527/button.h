#ifndef BUTTON_H
#define BUTTON_H

#include <DirectXMath.h>

#include <memory>
#include <string>

namespace hal
{
	class DebugText;
}

enum class ButtonClickSound
{
	Confirm,
	Back,
	None,
};

void Button_PlayNavigateSound();
void Button_PlayConfirmSound();
void Button_PlayBackSound();

class cButton
{
public:
	cButton() = default;
	~cButton();

	cButton(const cButton&) = delete;
	cButton& operator=(const cButton&) = delete;

	bool Initialize(
		const char* label,
		const DirectX::XMFLOAT2& center,
		const DirectX::XMFLOAT2& size);
	void Finalize();

	// Returns true when a press that started on the button is released on it.
	bool Update();
	void Draw();

	void SetSelected(bool selected);
	void SetEnabled(bool enabled);
	void SetClickSound(ButtonClickSound sound);
	bool IsHovered() const;
	bool IsSelected() const;

private:
	bool Contains(float x, float y) const;

	std::string m_Label;
	DirectX::XMFLOAT2 m_Center{};
	DirectX::XMFLOAT2 m_Size{};
	std::unique_ptr<hal::DebugText> m_LabelText;
	bool m_IsHovered{ false };
	bool m_IsPressed{ false };
	bool m_IsSelected{ false };
	bool m_IsEnabled{ true };
	bool m_HasSharedTextures{ false };
	ButtonClickSound m_ClickSound{ ButtonClickSound::Confirm };
};

#endif // BUTTON_H
