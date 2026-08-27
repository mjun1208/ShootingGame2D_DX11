#include "clear_scene.h"

#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "scene_manager.h"
#include "sprite.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
	constexpr float UI_FONT_GLYPH_WIDTH = 20.0f;
	constexpr int NEW_RUN_BUTTON_INDEX = 0;
	constexpr int TITLE_BUTTON_INDEX = 1;
	constexpr char HEADER_LABEL[] = "RUN CLEAR";
	constexpr char INPUT_HINT[] = "UP/DOWN  SELECT    ENTER  CONFIRM";

	std::string FormatClearTime(float clear_time_seconds)
	{
		const int total_centiseconds = static_cast<int>(std::round(
			std::max(clear_time_seconds, 0.0f) * 100.0f));
		const int centiseconds = total_centiseconds % 100;
		const int total_seconds = total_centiseconds / 100;
		const int seconds = total_seconds % 60;
		const int total_minutes = total_seconds / 60;
		const int minutes = total_minutes % 60;
		const int hours = total_minutes / 60;

		std::ostringstream stream;
		stream << "CLEAR TIME  ";
		if (hours > 0)
		{
			stream << std::setfill('0') << std::setw(2) << hours << ':';
		}
		stream << std::setfill('0') << std::setw(2) << minutes << ':'
			<< std::setw(2) << seconds << '.'
			<< std::setw(2) << centiseconds;
		return stream.str();
	}

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

ClearScene::ClearScene(float clear_time_seconds)
	: m_ClearTimeSeconds(std::max(clear_time_seconds, 0.0f))
{
}

ClearScene::~ClearScene() = default;

bool ClearScene::Initialize()
{
	m_BackgroundTextureID = Texture_Load(
		L"asset/texture/map/dungeon/ground/floor_dark.png",
		false);
	if (m_BackgroundTextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}

	m_HeaderText = CreateCenteredText(HEADER_LABEL, 190.0f, 32.0f, 26.0f);
	m_TimeText = CreateCenteredText(
		FormatClearTime(m_ClearTimeSeconds),
		320.0f,
		32.0f,
		22.0f);
	m_HintText = CreateCenteredText(INPUT_HINT, 690.0f, 32.0f, 20.0f);

	m_NewRunButton.Initialize(
		"NEW RUN",
		{ SCREEN_WIDTH * 0.5f, 500.0f },
		{ 345.0f, 90.0f });
	m_TitleButton.Initialize(
		"TITLE",
		{ SCREEN_WIDTH * 0.5f, 620.0f },
		{ 345.0f, 90.0f });

	SetSelectedButton(NEW_RUN_BUTTON_INDEX);
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
	m_IsTransitioning = false;
	return true;
}

void ClearScene::Finalize()
{
	m_TitleButton.Finalize();
	m_NewRunButton.Finalize();
	m_HintText.reset();
	m_TimeText.reset();
	m_HeaderText.reset();
	Texture_Release(m_BackgroundTextureID);
	m_BackgroundTextureID = TEXTURE_INVALID_ID;
}

void ClearScene::Update(float delta_time)
{
	(void)delta_time;
	if (m_IsTransitioning)
	{
		return;
	}

	const bool new_run_clicked = m_NewRunButton.Update();
	const bool title_clicked = m_TitleButton.Update();
	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved = mouse_x != m_LastMouseX || mouse_y != m_LastMouseY;
	m_LastMouseX = mouse_x;
	m_LastMouseY = mouse_y;
	if (mouse_moved && m_NewRunButton.IsHovered())
	{
		SetSelectedButton(NEW_RUN_BUTTON_INDEX);
	}
	else if (mouse_moved && m_TitleButton.IsHovered())
	{
		SetSelectedButton(TITLE_BUTTON_INDEX);
	}

	if (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		Button_PlayNavigateSound();
		SetSelectedButton(NEW_RUN_BUTTON_INDEX);
	}
	if (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		Button_PlayNavigateSound();
		SetSelectedButton(TITLE_BUTTON_INDEX);
	}

	if (new_run_clicked)
	{
		SetSelectedButton(NEW_RUN_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (title_clicked)
	{
		SetSelectedButton(TITLE_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE))
	{
		Button_PlayConfirmSound();
		ActivateSelectedButton();
	}
}

void ClearScene::Draw()
{
	if (m_BackgroundTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	Sprite_ResetViewMatrix();
	Sprite_SetFilter(kPOINT);
	Sprite_DrawSized(
		m_BackgroundTextureID,
		SCREEN_WIDTH * 0.5f,
		SCREEN_HEIGHT * 0.5f,
		static_cast<float>(SCREEN_WIDTH),
		static_cast<float>(SCREEN_HEIGHT),
		{ 0.22f, 0.07f, 0.06f, 1.0f });
	Sprite_DrawSized(
		m_BackgroundTextureID,
		SCREEN_WIDTH * 0.5f,
		430.0f,
		760.0f,
		590.0f,
		{ 0.05f, 0.04f, 0.06f, 0.92f });
	Sprite_DrawSized(
		m_BackgroundTextureID,
		SCREEN_WIDTH * 0.5f,
		150.0f,
		760.0f,
		5.0f,
		{ 1.0f, 0.35f, 0.08f, 1.0f });

	if (m_HeaderText)
	{
		m_HeaderText->Clear();
		m_HeaderText->SetText(HEADER_LABEL, { 1.0f, 0.50f, 0.18f, 1.0f });
		m_HeaderText->Draw();
	}
	if (m_TimeText)
	{
		const std::string clear_time = FormatClearTime(m_ClearTimeSeconds);
		m_TimeText->Clear();
		m_TimeText->SetText(clear_time.c_str(), { 0.95f, 0.90f, 0.76f, 1.0f });
		m_TimeText->Draw();
	}

	m_NewRunButton.Draw();
	m_TitleButton.Draw();

	if (m_HintText)
	{
		m_HintText->Clear();
		m_HintText->SetText(
			INPUT_HINT,
			{ 0.55f, 0.53f, 0.58f, 1.0f });
		m_HintText->Draw();
	}
}

void ClearScene::ActivateSelectedButton()
{
	m_IsTransitioning = true;
	SceneManager_ChangeScene(
		m_SelectedButton == NEW_RUN_BUTTON_INDEX ?
			SceneID::Ingame : SceneID::Title);
}

void ClearScene::SetSelectedButton(int index)
{
	m_SelectedButton = index == TITLE_BUTTON_INDEX ?
		TITLE_BUTTON_INDEX : NEW_RUN_BUTTON_INDEX;
	m_NewRunButton.SetSelected(m_SelectedButton == NEW_RUN_BUTTON_INDEX);
	m_TitleButton.SetSelected(m_SelectedButton == TITLE_BUTTON_INDEX);
}
