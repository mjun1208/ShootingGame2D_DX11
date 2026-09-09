#include "button.h"

#include "Audio.h"
#include "config.h"
#include "bitmap_text.h"
#include "input_mouse.h"
#include "sprite.h"
#include "texture.h"

#include <algorithm>

namespace
{

	int g_ButtonNormalTextureID = TEXTURE_INVALID_ID;
	int g_ButtonHoveredTextureID = TEXTURE_INVALID_ID;
	int g_ButtonPressedTextureID = TEXTURE_INVALID_ID;
	int g_ButtonTextureUserCount = 0;
	int g_UiNavigateAudioID = -1;
	int g_UiConfirmAudioID = -1;
	int g_UiBackAudioID = -1;

	bool AcquireButtonTextures()
	{
		if (g_ButtonTextureUserCount == 0)
		{
			g_ButtonNormalTextureID = Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_menubutton.png", false);
			g_ButtonHoveredTextureID =
			    Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_menubutton-hovered.png", false);
			g_ButtonPressedTextureID =
			    Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_menubutton-pressed.png", false);
			if (g_ButtonNormalTextureID == TEXTURE_INVALID_ID || g_ButtonHoveredTextureID == TEXTURE_INVALID_ID ||
			    g_ButtonPressedTextureID == TEXTURE_INVALID_ID)
			{
				Texture_Release(g_ButtonNormalTextureID);
				Texture_Release(g_ButtonHoveredTextureID);
				Texture_Release(g_ButtonPressedTextureID);
				g_ButtonNormalTextureID = TEXTURE_INVALID_ID;
				g_ButtonHoveredTextureID = TEXTURE_INVALID_ID;
				g_ButtonPressedTextureID = TEXTURE_INVALID_ID;
				return false;
			}

			if (g_UiNavigateAudioID < 0)
			{
				g_UiNavigateAudioID = Audio_Load("asset/sound/ui_navigate.wav");
			}
			if (g_UiConfirmAudioID < 0)
			{
				g_UiConfirmAudioID = Audio_Load("asset/sound/ui_confirm.wav");
			}
			if (g_UiBackAudioID < 0)
			{
				g_UiBackAudioID = Audio_Load("asset/sound/ui_back.wav");
			}
		}

		++g_ButtonTextureUserCount;
		return true;
	}

	void ReleaseButtonTextures()
	{
		if (g_ButtonTextureUserCount <= 0)
		{
			return;
		}

		--g_ButtonTextureUserCount;
		if (g_ButtonTextureUserCount == 0)
		{
			Texture_Release(g_ButtonNormalTextureID);
			Texture_Release(g_ButtonHoveredTextureID);
			Texture_Release(g_ButtonPressedTextureID);
			g_ButtonNormalTextureID = TEXTURE_INVALID_ID;
			g_ButtonHoveredTextureID = TEXTURE_INVALID_ID;
			g_ButtonPressedTextureID = TEXTURE_INVALID_ID;
		}
	}

	void DrawButtonSkin(int texture_id, const DirectX::XMFLOAT2& center, const DirectX::XMFLOAT2& size, float offset_y,
	                    const DirectX::XMFLOAT4& color)
	{
		static constexpr int CapWidth = 18;
		static constexpr int TextureWidth = 69;
		static constexpr int RightCapX = TextureWidth - CapWidth;
		static constexpr int CenterSourceX = 19;
		static constexpr int TextureHeight = 18;

		const float pixel_scale = size.y / TextureHeight;
		const float cap_width = CapWidth * pixel_scale;
		const float center_width = std::max(size.x - cap_width * 2.0f, pixel_scale);
		const float left = center.x - size.x * 0.5f;

		Sprite_DrawRegion(texture_id, { left + cap_width * 0.5f, center.y + offset_y }, { cap_width, size.y },
		                  { 0, 0, CapWidth, TextureHeight }, color);
		Sprite_DrawRegion(texture_id, { left + cap_width + center_width * 0.5f, center.y + offset_y },
		                  { center_width, size.y }, { CenterSourceX, 0, 1, TextureHeight }, color);
		Sprite_DrawRegion(texture_id, { left + cap_width + center_width + cap_width * 0.5f, center.y + offset_y },
		                  { cap_width, size.y }, { RightCapX, 0, CapWidth, TextureHeight }, color);
	}

} // namespace

void Button_PlayNavigateSound()
{
	Audio_Play(g_UiNavigateAudioID);
}

void Button_PlayConfirmSound()
{
	Audio_Play(g_UiConfirmAudioID);
}

void Button_PlayBackSound()
{
	Audio_Play(g_UiBackAudioID);
}

cButton::~cButton() = default;

bool cButton::Initialize(const char* label, const DirectX::XMFLOAT2& center, const DirectX::XMFLOAT2& size)
{
	static constexpr float CharacterSpacing = 21.0f;
	static constexpr float CharacterHeight = 32.0f;
	static constexpr float CharacterWidth = 20.0f;

	Finalize();
	if (!AcquireButtonTextures())
	{
		return false;
	}
	m_HasSharedTextures = true;

	m_Label = label ? label : "";
	m_Center = center;
	m_Size = {
		std::max(size.x, 1.0f),
		std::max(size.y, 1.0f),
	};
	m_IsEnabled = true;
	m_IsHovered = Contains(static_cast<float>(InputMouse_GetX()), static_cast<float>(InputMouse_GetY()));

	const float label_y = m_Center.y - CharacterHeight * 0.5f;
	m_LabelText = hal::CreateCenteredText(m_Label.c_str(), m_Center.x, label_y, CharacterSpacing, CharacterHeight,
	                                      L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png", CharacterWidth);

	return true;
}

void cButton::Finalize()
{
	if (m_HasSharedTextures)
	{
		ReleaseButtonTextures();
		m_HasSharedTextures = false;
	}
	m_LabelText.reset();
	m_Label.clear();
	m_IsHovered = false;
	m_IsPressed = false;
	m_IsSelected = false;
	m_IsEnabled = true;
	m_ClickSound = ButtonClickSound::Confirm;
}

bool cButton::Update()
{
	if (!m_IsEnabled)
	{
		m_IsHovered = false;
		m_IsPressed = false;
		return false;
	}

	const bool was_hovered = m_IsHovered;
	m_IsHovered = Contains(static_cast<float>(InputMouse_GetX()), static_cast<float>(InputMouse_GetY()));
	if (m_IsHovered && !was_hovered)
	{
		Button_PlayNavigateSound();
	}

	if (InputMouse_IsTrigger(MOUSE_BUTTON_LEFT))
	{
		m_IsPressed = m_IsHovered;
	}

	const bool clicked = InputMouse_IsRelease(MOUSE_BUTTON_LEFT) && m_IsPressed && m_IsHovered;

	if (InputMouse_IsRelease(MOUSE_BUTTON_LEFT))
	{
		m_IsPressed = false;
	}
	if (clicked)
	{
		switch (m_ClickSound)
		{
		case ButtonClickSound::Confirm:
			Button_PlayConfirmSound();
			break;
		case ButtonClickSound::Back:
			Button_PlayBackSound();
			break;
		case ButtonClickSound::None:
			break;
		}
	}

	return clicked;
}

void cButton::Draw()
{
	if (!m_HasSharedTextures || g_ButtonNormalTextureID == TEXTURE_INVALID_ID ||
	    g_ButtonHoveredTextureID == TEXTURE_INVALID_ID || g_ButtonPressedTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const bool highlighted = m_IsSelected || m_IsHovered;
	const float pressed_offset_y = m_IsPressed ? 3.0f : 0.0f;
	const DirectX::XMFLOAT4 button_color =
	    !m_IsEnabled ? DirectX::XMFLOAT4{ 0.48f, 0.48f, 0.52f, 0.82f } : DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
	const int button_texture_id = m_IsPressed   ? g_ButtonPressedTextureID
	                              : highlighted ? g_ButtonHoveredTextureID
	                                            : g_ButtonNormalTextureID;

	DrawButtonSkin(button_texture_id, m_Center, m_Size, pressed_offset_y, button_color);

	if (m_LabelText)
	{
		const DirectX::XMFLOAT4 text_color = !m_IsEnabled  ? DirectX::XMFLOAT4{ 0.50f, 0.50f, 0.54f, 1.0f }
		                                     : highlighted ? DirectX::XMFLOAT4{ 1.0f, 0.88f, 0.50f, 1.0f }
		                                                   : DirectX::XMFLOAT4{ 0.94f, 0.70f, 0.28f, 1.0f };
		m_LabelText->Clear();
		m_LabelText->SetText(m_Label.c_str(), text_color);
		m_LabelText->Draw();
	}
}

void cButton::SetSelected(bool selected)
{
	m_IsSelected = selected;
}

void cButton::SetEnabled(bool enabled)
{
	m_IsEnabled = enabled;
	if (!m_IsEnabled)
	{
		m_IsHovered = false;
		m_IsPressed = false;
	}
}

void cButton::SetClickSound(ButtonClickSound sound)
{
	m_ClickSound = sound;
}

bool cButton::IsHovered() const
{
	return m_IsHovered;
}

bool cButton::IsSelected() const
{
	return m_IsSelected;
}

bool cButton::Contains(float x, float y) const
{
	const float half_width = m_Size.x * 0.5f;
	const float half_height = m_Size.y * 0.5f;
	return x >= m_Center.x - half_width && x <= m_Center.x + half_width && y >= m_Center.y - half_height &&
	       y <= m_Center.y + half_height;
}
