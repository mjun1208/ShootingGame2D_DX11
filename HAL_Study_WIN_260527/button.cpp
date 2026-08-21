#include "button.h"

#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "input_mouse.h"
#include "sprite.h"
#include "texture.h"

#include <algorithm>

namespace
{
	constexpr float LABEL_CHARACTER_WIDTH = 20.0f;
	constexpr float LABEL_CHARACTER_HEIGHT = 32.0f;
	constexpr float LABEL_CHARACTER_SPACING = 21.0f;
	constexpr int BUTTON_TEXTURE_WIDTH = 69;
	constexpr int BUTTON_TEXTURE_HEIGHT = 18;
	constexpr int BUTTON_CAP_WIDTH = 18;
	constexpr int BUTTON_CENTER_SOURCE_X = 19;
	constexpr int BUTTON_RIGHT_CAP_X = BUTTON_TEXTURE_WIDTH - BUTTON_CAP_WIDTH;
	int g_ButtonNormalTextureID = TEXTURE_INVALID_ID;
	int g_ButtonHoveredTextureID = TEXTURE_INVALID_ID;
	int g_ButtonPressedTextureID = TEXTURE_INVALID_ID;
	int g_ButtonTextureUserCount = 0;

	bool AcquireButtonTextures()
	{
		if (g_ButtonTextureUserCount == 0)
		{
			g_ButtonNormalTextureID = Texture_Load(
				L"asset/dark_rpg_gui/dfgui_menubutton.png",
				false);
			g_ButtonHoveredTextureID = Texture_Load(
				L"asset/dark_rpg_gui/dfgui_menubutton-hovered.png",
				false);
			g_ButtonPressedTextureID = Texture_Load(
				L"asset/dark_rpg_gui/dfgui_menubutton-pressed.png",
				false);
			if (g_ButtonNormalTextureID == TEXTURE_INVALID_ID ||
				g_ButtonHoveredTextureID == TEXTURE_INVALID_ID ||
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

	void DrawButtonSkin(
		int texture_id,
		const DirectX::XMFLOAT2& center,
		const DirectX::XMFLOAT2& size,
		float offset_y,
		const DirectX::XMFLOAT4& color)
	{
		const float pixel_scale = size.y / BUTTON_TEXTURE_HEIGHT;
		const float cap_width = BUTTON_CAP_WIDTH * pixel_scale;
		const float center_width = std::max(size.x - cap_width * 2.0f, pixel_scale);
		const float left = center.x - size.x * 0.5f;

		Sprite_DrawRegion(
			texture_id,
			left + cap_width * 0.5f,
			center.y + offset_y,
			cap_width,
			size.y,
			0,
			0,
			BUTTON_CAP_WIDTH,
			BUTTON_TEXTURE_HEIGHT,
			color);
		Sprite_DrawRegion(
			texture_id,
			left + cap_width + center_width * 0.5f,
			center.y + offset_y,
			center_width,
			size.y,
			BUTTON_CENTER_SOURCE_X,
			0,
			1,
			BUTTON_TEXTURE_HEIGHT,
			color);
		Sprite_DrawRegion(
			texture_id,
			left + cap_width + center_width + cap_width * 0.5f,
			center.y + offset_y,
			cap_width,
			size.y,
			BUTTON_RIGHT_CAP_X,
			0,
			BUTTON_CAP_WIDTH,
			BUTTON_TEXTURE_HEIGHT,
			color);
	}

	float GetCenteredTextOffsetX(const std::string& text, float center_x)
	{
		if (text.empty())
		{
			return center_x;
		}

		const float text_width = LABEL_CHARACTER_WIDTH +
			(static_cast<float>(text.size()) - 1.0f) * LABEL_CHARACTER_SPACING;
		return center_x - text_width * 0.5f;
	}
}

cButton::~cButton() = default;

bool cButton::Initialize(
	const char* label,
	const DirectX::XMFLOAT2& center,
	const DirectX::XMFLOAT2& size)
{
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

	const float label_x = GetCenteredTextOffsetX(m_Label, m_Center.x);
	const float label_y = m_Center.y - LABEL_CHARACTER_HEIGHT * 0.5f;
	m_LabelText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(),
		Direct3D_GetDeviceContext(),
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		label_x,
		label_y,
		1,
		0,
		LABEL_CHARACTER_HEIGHT,
		LABEL_CHARACTER_SPACING);

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
}

bool cButton::Update()
{
	if (!m_IsEnabled)
	{
		m_IsHovered = false;
		m_IsPressed = false;
		return false;
	}

	m_IsHovered = Contains(
		static_cast<float>(InputMouse_GetX()),
		static_cast<float>(InputMouse_GetY()));

	if (InputMouse_IsTrigger(MOUSE_BUTTON_LEFT))
	{
		m_IsPressed = m_IsHovered;
	}

	const bool clicked =
		InputMouse_IsRelease(MOUSE_BUTTON_LEFT) &&
		m_IsPressed &&
		m_IsHovered;

	if (InputMouse_IsRelease(MOUSE_BUTTON_LEFT))
	{
		m_IsPressed = false;
	}

	return clicked;
}

void cButton::Draw()
{
	if (!m_HasSharedTextures ||
		g_ButtonNormalTextureID == TEXTURE_INVALID_ID ||
		g_ButtonHoveredTextureID == TEXTURE_INVALID_ID ||
		g_ButtonPressedTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const bool highlighted = m_IsSelected || m_IsHovered;
	const float pressed_offset_y = m_IsPressed ? 3.0f : 0.0f;
	const DirectX::XMFLOAT4 button_color = !m_IsEnabled ?
		DirectX::XMFLOAT4{ 0.48f, 0.48f, 0.52f, 0.82f } :
		DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
	const int button_texture_id = m_IsPressed ?
		g_ButtonPressedTextureID :
		highlighted ? g_ButtonHoveredTextureID : g_ButtonNormalTextureID;

	DrawButtonSkin(
		button_texture_id,
		m_Center,
		m_Size,
		pressed_offset_y,
		button_color);

	if (m_LabelText)
	{
		const DirectX::XMFLOAT4 text_color = !m_IsEnabled ?
			DirectX::XMFLOAT4{ 0.50f, 0.50f, 0.54f, 1.0f } :
			highlighted ?
				DirectX::XMFLOAT4{ 1.0f, 0.88f, 0.50f, 1.0f } :
				DirectX::XMFLOAT4{ 0.94f, 0.70f, 0.28f, 1.0f };
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
	return x >= m_Center.x - half_width &&
		x <= m_Center.x + half_width &&
		y >= m_Center.y - half_height &&
		y <= m_Center.y + half_height;
}
