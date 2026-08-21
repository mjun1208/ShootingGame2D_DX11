#include "game_over_scene.h"

#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "scene_manager.h"
#include "sprite.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace
{
	constexpr float UI_FONT_GLYPH_WIDTH = 20.0f;
	constexpr int RETRY_BUTTON_INDEX = 0;
	constexpr int TITLE_BUTTON_INDEX = 1;
	constexpr char HEADER_LABEL[] = "GAME OVER";
	constexpr char SUBTITLE_LABEL[] = "THE DUNGEON CLAIMED ANOTHER SOUL";
	constexpr char INPUT_HINT[] = "UP/DOWN  SELECT    ENTER  CONFIRM";

	std::unique_ptr<hal::DebugText> CreateCenteredText(
		const std::string& text,
		float offset_y,
		float glyph_size,
		float character_spacing)
	{
		const float text_width = text.empty() ? 0.0f :
			UI_FONT_GLYPH_WIDTH +
			(static_cast<float>(text.size()) - 1.0f) * character_spacing;
		return std::make_unique<hal::DebugText>(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			SCREEN_WIDTH * 0.5f - text_width * 0.5f,
			offset_y,
			1,
			0,
			glyph_size,
			character_spacing);
	}
}

GameOverScene::~GameOverScene() = default;

bool GameOverScene::Initialize()
{
	m_BackgroundTextureID = Texture_Load(
		L"asset/texture/map/dungeon/ground/floor_dark.png",
		false);
	if (m_BackgroundTextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}

	m_HeaderText = CreateCenteredText(HEADER_LABEL, 205.0f, 32.0f, 26.0f);
	m_SubtitleText = CreateCenteredText(SUBTITLE_LABEL, 330.0f, 24.0f, 20.0f);
	m_HintText = CreateCenteredText(INPUT_HINT, 720.0f, 32.0f, 20.0f);
	m_RetryButton.Initialize(
		"RETRY",
		{ SCREEN_WIDTH * 0.5f, 510.0f },
		{ 345.0f, 90.0f });
	m_TitleButton.Initialize(
		"TITLE",
		{ SCREEN_WIDTH * 0.5f, 630.0f },
		{ 345.0f, 90.0f });

	SetSelectedButton(RETRY_BUTTON_INDEX);
	m_ElapsedTime = 0.0f;
	m_LastMouseX = InputMouse_GetX();
	m_LastMouseY = InputMouse_GetY();
	m_IsTransitioning = false;
	return true;
}

void GameOverScene::Finalize()
{
	m_TitleButton.Finalize();
	m_RetryButton.Finalize();
	m_HintText.reset();
	m_SubtitleText.reset();
	m_HeaderText.reset();
	Texture_Release(m_BackgroundTextureID);
	m_BackgroundTextureID = TEXTURE_INVALID_ID;
}

void GameOverScene::Update(float delta_time)
{
	m_ElapsedTime += std::max(delta_time, 0.0f);
	if (m_IsTransitioning)
	{
		return;
	}

	const bool retry_clicked = m_RetryButton.Update();
	const bool title_clicked = m_TitleButton.Update();
	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved = mouse_x != m_LastMouseX || mouse_y != m_LastMouseY;
	m_LastMouseX = mouse_x;
	m_LastMouseY = mouse_y;
	if (mouse_moved && m_RetryButton.IsHovered())
	{
		SetSelectedButton(RETRY_BUTTON_INDEX);
	}
	else if (mouse_moved && m_TitleButton.IsHovered())
	{
		SetSelectedButton(TITLE_BUTTON_INDEX);
	}

	if (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		SetSelectedButton(RETRY_BUTTON_INDEX);
	}
	if (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		SetSelectedButton(TITLE_BUTTON_INDEX);
	}

	if (retry_clicked)
	{
		SetSelectedButton(RETRY_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (title_clicked)
	{
		SetSelectedButton(TITLE_BUTTON_INDEX);
		ActivateSelectedButton();
	}
	else if (InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE))
	{
		ActivateSelectedButton();
	}
}

void GameOverScene::Draw()
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
		{ 0.08f, 0.01f, 0.015f, 1.0f });
	Sprite_DrawSized(
		m_BackgroundTextureID,
		SCREEN_WIDTH * 0.5f,
		445.0f,
		780.0f,
		610.0f,
		{ 0.025f, 0.015f, 0.02f, 0.94f });

	const float pulse = 0.78f + 0.22f *
		(0.5f + 0.5f * std::sin(m_ElapsedTime * 3.5f));
	Sprite_DrawSized(
		m_BackgroundTextureID,
		SCREEN_WIDTH * 0.5f,
		160.0f,
		780.0f,
		6.0f,
		{ pulse, 0.02f, 0.025f, 1.0f });

	if (m_HeaderText)
	{
		m_HeaderText->Clear();
		m_HeaderText->SetText(
			HEADER_LABEL,
			{ 1.0f, 0.12f * pulse, 0.10f * pulse, 1.0f });
		m_HeaderText->Draw();
	}
	if (m_SubtitleText)
	{
		m_SubtitleText->Clear();
		m_SubtitleText->SetText(
			SUBTITLE_LABEL,
			{ 0.72f, 0.60f, 0.60f, 1.0f });
		m_SubtitleText->Draw();
	}

	m_RetryButton.Draw();
	m_TitleButton.Draw();

	if (m_HintText)
	{
		m_HintText->Clear();
		m_HintText->SetText(INPUT_HINT, { 0.48f, 0.42f, 0.44f, 1.0f });
		m_HintText->Draw();
	}
}

void GameOverScene::ActivateSelectedButton()
{
	m_IsTransitioning = true;
	SceneManager_ChangeScene(
		m_SelectedButton == RETRY_BUTTON_INDEX ?
			SceneID::Ingame : SceneID::Title);
}

void GameOverScene::SetSelectedButton(int index)
{
	m_SelectedButton = index == TITLE_BUTTON_INDEX ?
		TITLE_BUTTON_INDEX : RETRY_BUTTON_INDEX;
	m_RetryButton.SetSelected(m_SelectedButton == RETRY_BUTTON_INDEX);
	m_TitleButton.SetSelected(m_SelectedButton == TITLE_BUTTON_INDEX);
}
