#include "title_scene.h"

#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "scene_manager.h"
#include "sprite.h"

#include <Windows.h>

#include <memory>
#include <string>

namespace
{
	constexpr float UI_FONT_GLYPH_WIDTH = 20.0f;
	constexpr int START_BUTTON_INDEX = 0;
	constexpr int HOW_TO_BUTTON_INDEX = 1;
	constexpr int EXIT_BUTTON_INDEX = 2;
	constexpr int BUTTON_COUNT = 3;
	constexpr char HOW_TO_HEADER[] = "HOW TO PLAY";
	constexpr char HOW_TO_BODY[] =
		"MOVE       W A S D\n"
		"AIM        MOUSE\n"
		"FIRE       LEFT CLICK\n"
		"DASH       SPACE + W A S D\n"
		"MAP        M\n"
		"USE EXIT   E\n"
		"\n"
		"CLEAR FOUR ROUNDS AND DEFEAT THE BOSS.";

	std::unique_ptr<hal::DebugText> CreateCenteredText(
		const std::string& text,
		float offset_y,
		float glyph_size,
		float character_spacing)
	{
		const float text_width = text.empty() ? 0.0f :
			UI_FONT_GLYPH_WIDTH +
			(static_cast<float>(text.size()) - 1.0f) * character_spacing;
		const float offset_x = SCREEN_WIDTH * 0.5f - text_width * 0.5f;
		return std::make_unique<hal::DebugText>(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			offset_x,
			offset_y,
			1,
			0,
			glyph_size,
			character_spacing);
	}
}

TitleScene::~TitleScene() = default;

bool TitleScene::Initialize()
{
	m_LogoTextureID = Texture_Load(L"asset/texture/Logo.png");
	m_UiTextureID = Texture_Load(
		L"asset/texture/map/dungeon/ground/floor_dark.png",
		false);
	if (m_LogoTextureID == TEXTURE_INVALID_ID ||
		m_UiTextureID == TEXTURE_INVALID_ID)
	{
		Finalize();
		return false;
	}

	m_StartButton.Initialize(
		"START",
		{ SCREEN_WIDTH * 0.5f, 535.0f },
		{ 345.0f, 90.0f });
	m_HowToButton.Initialize(
		"HOW TO",
		{ SCREEN_WIDTH * 0.5f, 650.0f },
		{ 345.0f, 90.0f });
	m_ExitButton.Initialize(
		"EXIT",
		{ SCREEN_WIDTH * 0.5f, 765.0f },
		{ 345.0f, 90.0f });
	m_BackButton.Initialize(
		"BACK",
		{ SCREEN_WIDTH * 0.5f, 770.0f },
		{ 345.0f, 90.0f });

	m_HowToHeaderText = CreateCenteredText(
		HOW_TO_HEADER,
		245.0f,
		32.0f,
		34.0f);
	m_HowToBodyText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(),
		Direct3D_GetDeviceContext(),
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		450.0f,
		340.0f,
		8,
		0,
		50.0f,
		22.0f);

	SetSelectedButton(START_BUTTON_INDEX);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
	m_ShowHowTo = false;
	m_IsTransitioning = false;
	m_BackButton.SetEnabled(false);
	return true;
}

void TitleScene::Finalize()
{
	m_BackButton.Finalize();
	m_ExitButton.Finalize();
	m_HowToButton.Finalize();
	m_StartButton.Finalize();
	m_HowToBodyText.reset();
	m_HowToHeaderText.reset();
	Texture_Release(m_UiTextureID);
	m_UiTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_LogoTextureID);
	m_LogoTextureID = TEXTURE_INVALID_ID;
}

void TitleScene::Update(float delta_time)
{
	(void)delta_time;
	if (m_IsTransitioning)
	{
		return;
	}

	if (m_ShowHowTo)
	{
		UpdateHowTo();
		return;
	}

	UpdateMainMenu();
}

void TitleScene::Draw()
{
	if (m_LogoTextureID == TEXTURE_INVALID_ID ||
		m_UiTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	Sprite_ResetViewMatrix();
	Sprite_SetFilter(kPOINT);
	Sprite_DrawSized(
		m_UiTextureID,
		SCREEN_WIDTH * 0.5f,
		SCREEN_HEIGHT * 0.5f,
		static_cast<float>(SCREEN_WIDTH),
		static_cast<float>(SCREEN_HEIGHT),
		{ 0.18f, 0.07f, 0.08f, 1.0f });

	if (m_ShowHowTo)
	{
		DrawHowTo();
	}
	else
	{
		DrawMainMenu();
	}
}

void TitleScene::UpdateMainMenu()
{
	const bool start_clicked = m_StartButton.Update();
	const bool how_to_clicked = m_HowToButton.Update();
	const bool exit_clicked = m_ExitButton.Update();

	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved = mouse_x != m_LastMouseX || mouse_y != m_LastMouseY;
	m_LastMouseX = mouse_x;
	m_LastMouseY = mouse_y;
	if (mouse_moved && m_StartButton.IsHovered())
	{
		SetSelectedButton(START_BUTTON_INDEX);
	}
	else if (mouse_moved && m_HowToButton.IsHovered())
	{
		SetSelectedButton(HOW_TO_BUTTON_INDEX);
	}
	else if (mouse_moved && m_ExitButton.IsHovered())
	{
		SetSelectedButton(EXIT_BUTTON_INDEX);
	}

	if (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		SetSelectedButton((m_SelectedButton + BUTTON_COUNT - 1) % BUTTON_COUNT);
	}
	if (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		SetSelectedButton((m_SelectedButton + 1) % BUTTON_COUNT);
	}

	if (start_clicked)
	{
		SetSelectedButton(START_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (how_to_clicked)
	{
		SetSelectedButton(HOW_TO_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (exit_clicked)
	{
		SetSelectedButton(EXIT_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE))
	{
		ActivateSelectedButton();
	}
}

void TitleScene::UpdateHowTo()
{
	const bool back_clicked = m_BackButton.Update();
	if (back_clicked ||
		InputKeyboard_IsTrigger(KK_ENTER) ||
		InputKeyboard_IsTrigger(KK_SPACE) ||
		InputKeyboard_IsTrigger(KK_BACK))
	{
		CloseHowTo();
	}
}

void TitleScene::ActivateSelectedButton()
{
	switch (m_SelectedButton)
	{
	case START_BUTTON_INDEX:
		m_IsTransitioning = true;
		SceneManager_ChangeScene(SceneID::Ingame);
		break;
	case HOW_TO_BUTTON_INDEX:
		OpenHowTo();
		break;
	case EXIT_BUTTON_INDEX:
		if (HWND window = GetActiveWindow())
		{
			PostMessage(window, WM_CLOSE, 0, 0);
		}
		else
		{
			PostQuitMessage(0);
		}
		break;
	default:
		break;
	}
}

void TitleScene::SetSelectedButton(int index)
{
	m_SelectedButton = (index % BUTTON_COUNT + BUTTON_COUNT) % BUTTON_COUNT;
	m_StartButton.SetSelected(m_SelectedButton == START_BUTTON_INDEX);
	m_HowToButton.SetSelected(m_SelectedButton == HOW_TO_BUTTON_INDEX);
	m_ExitButton.SetSelected(m_SelectedButton == EXIT_BUTTON_INDEX);
}

void TitleScene::OpenHowTo()
{
	m_StartButton.SetEnabled(false);
	m_HowToButton.SetEnabled(false);
	m_ExitButton.SetEnabled(false);
	m_BackButton.SetEnabled(true);
	m_ShowHowTo = true;
	m_BackButton.SetSelected(true);
}

void TitleScene::CloseHowTo()
{
	m_BackButton.SetEnabled(false);
	m_StartButton.SetEnabled(true);
	m_HowToButton.SetEnabled(true);
	m_ExitButton.SetEnabled(true);
	m_ShowHowTo = false;
	m_BackButton.SetSelected(false);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
}

void TitleScene::DrawMainMenu()
{
	Sprite_Draw(
		m_LogoTextureID,
		SCREEN_WIDTH * 0.5f,
		240.0f,
		0.0f,
		0.64f);
	m_StartButton.Draw();
	m_HowToButton.Draw();
	m_ExitButton.Draw();
}

void TitleScene::DrawHowTo()
{
	Sprite_DrawSized(
		m_UiTextureID,
		SCREEN_WIDTH * 0.5f,
		510.0f,
		980.0f,
		650.0f,
		{ 0.05f, 0.04f, 0.06f, 0.96f });
	Sprite_DrawSized(
		m_UiTextureID,
		SCREEN_WIDTH * 0.5f,
		205.0f,
		980.0f,
		5.0f,
		{ 1.0f, 0.35f, 0.08f, 1.0f });

	if (m_HowToHeaderText)
	{
		m_HowToHeaderText->Clear();
		m_HowToHeaderText->SetText(
			HOW_TO_HEADER,
			{ 1.0f, 0.50f, 0.18f, 1.0f });
		m_HowToHeaderText->Draw();
	}
	if (m_HowToBodyText)
	{
		m_HowToBodyText->Clear();
		m_HowToBodyText->SetText(
			HOW_TO_BODY,
			{ 0.90f, 0.88f, 0.82f, 1.0f });
		m_HowToBodyText->Draw();
	}
	m_BackButton.Draw();
}
