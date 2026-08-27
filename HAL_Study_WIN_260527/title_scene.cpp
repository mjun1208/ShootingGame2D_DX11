#include "title_scene.h"

#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "game_data_manager.h"
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
	constexpr int CREDITS_BUTTON_INDEX = 2;
	constexpr int EXIT_BUTTON_INDEX = 3;
	constexpr int BUTTON_COUNT = 4;
	constexpr char HOW_TO_HEADER[] = "HOW TO PLAY";
	constexpr char HOW_TO_BODY[] =
		"MOVE          W A S D\n"
		"ATTACK        AUTOMATIC - NEAREST ENEMY\n"
		"DASH          SPACE\n"
		"TIME STOP     Q - DASH UP TO 3 TIMES\n"
		"FAST RUN      AUTOMATIC OUT OF COMBAT\n"
		"MAP           M\n"
		"INTERACT      F - CHEST / EXIT\n"
		"PAUSE         ESC\n"
		"\n"
		"OPEN CHESTS TO UNLOCK NEW WEAPONS.\n"
		"LEVEL UP AND CLEAR ROUNDS TO UPGRADE THEM.\n"
		"CLEAR FOUR ROUNDS; EACH ENDS WITH A BOSS.";
	constexpr char CREDITS_HEADER[] = "CREDITS";
	constexpr char CREDITS_BODY[] =
		"GAME DESIGN AND PROGRAMMING\n"
		"MINJUN\n"
		"\n"
		"CTHULHU BOSS ART: CHIERIT\n"
		"LICENSE: CC BY 4.0\n"
		"CHIERIT.ITCH.IO/FREE-CTHULU\n"
		"\n"
		"UI FONT: PIXELMPLUS12-BOLD\n"
		"FONT AUTHORS: ITOUHIRO / M+ FONTS PROJECT\n"
		"LICENSE: M+ FONT LICENSE\n"
		"\n"
		"OTHER ASSETS: SEE INCLUDED LICENSE FILES\n"
		"\n"
		"(C) 2026 MINJUN";

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
	if (!GameDataManager::GetInstance().LoadAll())
	{
		return false;
	}

	m_LogoTextureID = Texture_Load(
		L"asset/texture/Logo_DungeonSurvivor.png");
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
		{ SCREEN_WIDTH * 0.5f, 500.0f },
		{ 345.0f, 78.0f });
	m_HowToButton.Initialize(
		"HOW TO",
		{ SCREEN_WIDTH * 0.5f, 595.0f },
		{ 345.0f, 78.0f });
	m_CreditsButton.Initialize(
		"CREDITS",
		{ SCREEN_WIDTH * 0.5f, 690.0f },
		{ 345.0f, 78.0f });
	m_ExitButton.Initialize(
		"EXIT",
		{ SCREEN_WIDTH * 0.5f, 785.0f },
		{ 345.0f, 78.0f });
	m_BackButton.Initialize(
		"BACK",
		{ SCREEN_WIDTH * 0.5f, 770.0f },
		{ 345.0f, 90.0f });
	m_BackButton.SetClickSound(ButtonClickSound::Back);

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
		360.0f,
		305.0f,
		12,
		0,
		35.0f,
		18.0f);
	m_CreditsHeaderText = CreateCenteredText(
		CREDITS_HEADER,
		205.0f,
		32.0f,
		34.0f);
	m_CreditsBodyText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(),
		Direct3D_GetDeviceContext(),
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		330.0f,
		275.0f,
		16,
		0,
		32.0f,
		20.0f);

	SetSelectedButton(START_BUTTON_INDEX);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
	m_ShowHowTo = false;
	m_ShowCredits = false;
	m_IsTransitioning = false;
	m_BackButton.SetEnabled(false);
	return true;
}

void TitleScene::Finalize()
{
	m_BackButton.Finalize();
	m_ExitButton.Finalize();
	m_CreditsButton.Finalize();
	m_HowToButton.Finalize();
	m_StartButton.Finalize();
	m_CreditsBodyText.reset();
	m_CreditsHeaderText.reset();
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
	if (m_ShowCredits)
	{
		UpdateCredits();
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
	else if (m_ShowCredits)
	{
		DrawCredits();
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
	const bool credits_clicked = m_CreditsButton.Update();
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
	else if (mouse_moved && m_CreditsButton.IsHovered())
	{
		SetSelectedButton(CREDITS_BUTTON_INDEX);
	}
	else if (mouse_moved && m_ExitButton.IsHovered())
	{
		SetSelectedButton(EXIT_BUTTON_INDEX);
	}

	if (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		Button_PlayNavigateSound();
		SetSelectedButton((m_SelectedButton + BUTTON_COUNT - 1) % BUTTON_COUNT);
	}
	if (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		Button_PlayNavigateSound();
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
	else if (credits_clicked)
	{
		SetSelectedButton(CREDITS_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (exit_clicked)
	{
		SetSelectedButton(EXIT_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE))
	{
		Button_PlayConfirmSound();
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
		if (!back_clicked)
		{
			Button_PlayBackSound();
		}
		CloseHowTo();
	}
}

void TitleScene::UpdateCredits()
{
	const bool back_clicked = m_BackButton.Update();
	if (back_clicked ||
		InputKeyboard_IsTrigger(KK_ENTER) ||
		InputKeyboard_IsTrigger(KK_SPACE) ||
		InputKeyboard_IsTrigger(KK_BACK))
	{
		if (!back_clicked)
		{
			Button_PlayBackSound();
		}
		CloseCredits();
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
	case CREDITS_BUTTON_INDEX:
		OpenCredits();
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
	m_CreditsButton.SetSelected(m_SelectedButton == CREDITS_BUTTON_INDEX);
	m_ExitButton.SetSelected(m_SelectedButton == EXIT_BUTTON_INDEX);
}

void TitleScene::OpenHowTo()
{
	m_StartButton.SetEnabled(false);
	m_HowToButton.SetEnabled(false);
	m_CreditsButton.SetEnabled(false);
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
	m_CreditsButton.SetEnabled(true);
	m_ExitButton.SetEnabled(true);
	m_ShowHowTo = false;
	m_BackButton.SetSelected(false);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
}

void TitleScene::OpenCredits()
{
	m_StartButton.SetEnabled(false);
	m_HowToButton.SetEnabled(false);
	m_CreditsButton.SetEnabled(false);
	m_ExitButton.SetEnabled(false);
	m_BackButton.SetEnabled(true);
	m_ShowCredits = true;
	m_BackButton.SetSelected(true);
}

void TitleScene::CloseCredits()
{
	m_BackButton.SetEnabled(false);
	m_StartButton.SetEnabled(true);
	m_HowToButton.SetEnabled(true);
	m_CreditsButton.SetEnabled(true);
	m_ExitButton.SetEnabled(true);
	m_ShowCredits = false;
	m_BackButton.SetSelected(false);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
}

void TitleScene::DrawMainMenu()
{
	Sprite_Draw(
		m_LogoTextureID,
		SCREEN_WIDTH * 0.5f,
		235.0f,
		0.0f,
		0.50f);
	m_StartButton.Draw();
	m_HowToButton.Draw();
	m_CreditsButton.Draw();
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

void TitleScene::DrawCredits()
{
	Sprite_DrawSized(
		m_UiTextureID,
		SCREEN_WIDTH * 0.5f,
		490.0f,
		980.0f,
		700.0f,
		{ 0.035f, 0.025f, 0.055f, 0.97f });
	Sprite_DrawSized(
		m_UiTextureID,
		SCREEN_WIDTH * 0.5f,
		170.0f,
		980.0f,
		5.0f,
		{ 0.58f, 0.18f, 1.0f, 1.0f });

	if (m_CreditsHeaderText)
	{
		m_CreditsHeaderText->Clear();
		m_CreditsHeaderText->SetText(
			CREDITS_HEADER,
			{ 0.78f, 0.48f, 1.0f, 1.0f });
		m_CreditsHeaderText->Draw();
	}
	if (m_CreditsBodyText)
	{
		m_CreditsBodyText->Clear();
		m_CreditsBodyText->SetText(
			CREDITS_BODY,
			{ 0.91f, 0.88f, 0.96f, 1.0f });
		m_CreditsBodyText->Draw();
	}
	m_BackButton.Draw();
}
