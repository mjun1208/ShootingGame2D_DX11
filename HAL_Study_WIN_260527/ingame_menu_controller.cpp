#include "ingame_menu_controller.h"

#include "button.h"
#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "game_data_manager.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "weapon_data.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace
{
	namespace Panel
	{
		constexpr int TextureSize = 24;
		constexpr int SourceBorder = 4;
		constexpr float DrawBorder = 16.0f;
	}

	namespace Pause
	{
		enum ButtonIndex
		{
			ResumeButton,
			RetryButton,
			TitleButton,
			ExitButton,
			ButtonCount,
		};

		constexpr float PanelWidth = 540.0f;
		constexpr float PanelHeight = 720.0f;
		constexpr float ButtonWidth = 360.0f;
		constexpr float ButtonHeight = 82.0f;
		constexpr float FirstButtonY = 360.0f;
		constexpr float ButtonGap = 110.0f;
	}

	namespace Augment
	{
		constexpr int ChoiceCount = 3;
		constexpr float PanelWidth = 1320.0f;
		constexpr float PanelHeight = 820.0f;
		constexpr float CardWidth = 340.0f;
		constexpr float CardHeight = 570.0f;
		constexpr float CardGap = 48.0f;
		constexpr float CardCenterY = 535.0f;
		constexpr float IconCenterY = 365.0f;
		constexpr float IconFrameSize = 156.0f;
		constexpr float IconMaximumSize = 116.0f;
		constexpr float WeaponNameY = 455.0f;
		constexpr float EffectDetailY = 535.0f;
		constexpr float SelectButtonY = 720.0f;
		constexpr float SelectButtonWidth = 230.0f;
		constexpr float SelectButtonHeight = 66.0f;

		float GetCardCenterX(int index)
		{
			return SCREEN_WIDTH * 0.5f +
				(static_cast<float>(index) - 1.0f) * (CardWidth + CardGap);
		}
	}

	std::unique_ptr<hal::DebugText> CreateCenteredTextAt(
		const char* text,
		float center_x,
		float y,
		float character_spacing,
		float glyph_height = 32.0f)
	{
		const std::size_t character_count = text ?
			std::char_traits<char>::length(text) : 0;
		const float glyph_width = 20.0f;
		const float text_width = character_count > 0 ?
			glyph_width +
				(static_cast<float>(character_count) - 1.0f) * character_spacing :
			0.0f;
		return std::make_unique<hal::DebugText>(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			center_x - text_width * 0.5f,
			y,
			1,
			0,
			glyph_height,
			character_spacing);
	}

	std::unique_ptr<hal::DebugText> CreateCenteredText(
		const char* text,
		float y,
		float character_spacing,
		float glyph_height = 32.0f)
	{
		return CreateCenteredTextAt(
			text, SCREEN_WIDTH * 0.5f, y, character_spacing, glyph_height);
	}

	void DrawDimmer(int texture_id, float alpha)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}
		Sprite_DrawRegion(
			texture_id,
			SCREEN_WIDTH * 0.5f,
			SCREEN_HEIGHT * 0.5f,
			static_cast<float>(SCREEN_WIDTH),
			static_cast<float>(SCREEN_HEIGHT),
			Panel::TextureSize / 2,
			Panel::TextureSize / 2,
			1,
			1,
			{ 0.10f, 0.06f, 0.12f, alpha });
	}

	void DrawPanelAt(
		int texture_id,
		float center_x,
		float center_y,
		float panel_width,
		float panel_height,
		const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 0.98f })
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}

		const float left = center_x - panel_width * 0.5f;
		const float top = center_y - panel_height * 0.5f;
		const float center_width = panel_width - Panel::DrawBorder * 2.0f;
		const float center_height = panel_height - Panel::DrawBorder * 2.0f;
		const int source_center_size =
			Panel::TextureSize - Panel::SourceBorder * 2;
		const std::array<float, 3> destination_widths = {
			Panel::DrawBorder, center_width, Panel::DrawBorder };
		const std::array<float, 3> destination_heights = {
			Panel::DrawBorder, center_height, Panel::DrawBorder };
		const std::array<int, 3> source_positions = {
			0, Panel::SourceBorder, Panel::TextureSize - Panel::SourceBorder };
		const std::array<int, 3> source_sizes = {
			Panel::SourceBorder, source_center_size, Panel::SourceBorder };

		float destination_y = top;
		for (int row = 0; row < 3; ++row)
		{
			float destination_x = left;
			for (int column = 0; column < 3; ++column)
			{
				Sprite_DrawRegion(
					texture_id,
					destination_x + destination_widths[column] * 0.5f,
					destination_y + destination_heights[row] * 0.5f,
					destination_widths[column],
					destination_heights[row],
					source_positions[column],
					source_positions[row],
					source_sizes[column],
					source_sizes[row],
					color);
				destination_x += destination_widths[column];
			}
			destination_y += destination_heights[row];
		}
	}

	void DrawPanel(int texture_id, float panel_width, float panel_height)
	{
		DrawPanelAt(
			texture_id,
			SCREEN_WIDTH * 0.5f,
			SCREEN_HEIGHT * 0.5f,
			panel_width,
			panel_height);
	}

	const char* GetWeaponName(BulletType type)
	{
		switch (type)
		{
		case BulletType::Fireball: return "FIREBALL";
		case BulletType::Lightning: return "LIGHTNING";
		case BulletType::Ricochet: return "RICOCHET";
		case BulletType::BezierHoming: return "MAGIC BULLET";
		case BulletType::OrbitBlade: return "ORBIT BLADE";
		case BulletType::Boomerang: return "BOOMERANG";
		case BulletType::Shotgun: return "SHOTGUN";
		case BulletType::MagicBlade: return "MAGIC BLADE";
		case BulletType::Count:
		default: return "NO WEAPON";
		}
	}

	const char* GetEffectDetail(IngameAugmentChoice choice)
	{
		switch (choice)
		{
		case IngameAugmentChoice::MultiShot: return "PROJECTILES +1";
		case IngameAugmentChoice::Overdrive: return "ATTACK SPEED +20%";
		case IngameAugmentChoice::Power: return "DAMAGE +20%";
		case IngameAugmentChoice::None:
		default: return "";
		}
	}
}

struct IngameMenuController::Impl
{
	int PanelTextureID{ TEXTURE_INVALID_ID };
	std::unique_ptr<hal::DebugText> PauseTitleText;
	std::unique_ptr<hal::DebugText> AugmentTitleText;
	std::unique_ptr<hal::DebugText> RoundRewardTitleText;
	std::unique_ptr<hal::DebugText> AugmentHelpText;
	cButton ResumeButton;
	cButton RetryButton;
	cButton TitleButton;
	cButton ExitButton;
	std::array<cButton, Augment::ChoiceCount> AugmentSelectButtons;
	std::array<IngameAugmentSelection, Augment::ChoiceCount> AugmentOptions{};
	std::array<std::unique_ptr<hal::DebugText>, Augment::ChoiceCount>
		AugmentWeaponTexts;
	std::array<std::unique_ptr<hal::DebugText>, Augment::ChoiceCount>
		AugmentDetailTexts;
	std::mt19937 AugmentRandomGenerator{ std::random_device{}() };
	int SelectedPauseButton{ Pause::ResumeButton };
	int SelectedAugment{ 0 };
	int LastMouseX{ 0 };
	int LastMouseY{ 0 };
	bool PauseOpen{ false };
	bool PauseTransitioning{ false };
	bool AugmentOpen{ false };
	IngameAugmentReason AugmentReason{ IngameAugmentReason::LevelUp };

	void SelectPauseButton(int index)
	{
		SelectedPauseButton =
			(index % Pause::ButtonCount + Pause::ButtonCount) % Pause::ButtonCount;
		ResumeButton.SetSelected(SelectedPauseButton == Pause::ResumeButton);
		RetryButton.SetSelected(SelectedPauseButton == Pause::RetryButton);
		TitleButton.SetSelected(SelectedPauseButton == Pause::TitleButton);
		ExitButton.SetSelected(SelectedPauseButton == Pause::ExitButton);
	}

	void SelectAugment(int index)
	{
		SelectedAugment =
			(index % Augment::ChoiceCount + Augment::ChoiceCount) %
			Augment::ChoiceCount;
		for (int i = 0; i < Augment::ChoiceCount; ++i)
		{
			AugmentSelectButtons[i].SetSelected(SelectedAugment == i);
		}
	}

	bool BuildAugmentOptions()
	{
		std::vector<IngameAugmentSelection> candidates;
		for (int weapon_index = 0;
			weapon_index < static_cast<int>(BulletType::Count);
			++weapon_index)
		{
			const BulletType weapon_type = static_cast<BulletType>(weapon_index);
			if (!GameBullet::IsWeaponOwned(weapon_type))
			{
				continue;
			}
			candidates.push_back(
				{ weapon_type, IngameAugmentChoice::MultiShot, AugmentReason });
			candidates.push_back(
				{ weapon_type, IngameAugmentChoice::Overdrive, AugmentReason });
			candidates.push_back(
				{ weapon_type, IngameAugmentChoice::Power, AugmentReason });
		}
		if (candidates.empty())
		{
			return false;
		}

		std::shuffle(
			candidates.begin(), candidates.end(), AugmentRandomGenerator);
		for (int i = 0; i < Augment::ChoiceCount; ++i)
		{
			AugmentOptions[i] = i < static_cast<int>(candidates.size()) ?
				candidates[i] : IngameAugmentSelection{};
			const float card_x = Augment::GetCardCenterX(i);
			AugmentWeaponTexts[i] = CreateCenteredTextAt(
				GetWeaponName(AugmentOptions[i].WeaponType),
				card_x,
				Augment::WeaponNameY,
				18.0f,
				30.0f);
			AugmentDetailTexts[i] = CreateCenteredTextAt(
				GetEffectDetail(AugmentOptions[i].Choice),
				card_x,
				Augment::EffectDetailY,
				16.0f,
				28.0f);
			AugmentSelectButtons[i].SetEnabled(
				AugmentOptions[i].Choice != IngameAugmentChoice::None);
		}
		return true;
	}
};

IngameMenuController::IngameMenuController()
	: m_Impl(std::make_unique<Impl>())
{
}

IngameMenuController::~IngameMenuController() = default;

bool IngameMenuController::Initialize()
{
	Finalize();
	m_Impl->PanelTextureID = Texture_Load(
		L"asset/dark_rpg_gui/dfgui_button-empty.png", false);
	const bool pause_buttons_ready =
		m_Impl->ResumeButton.Initialize(
			"RESUME",
			{ SCREEN_WIDTH * 0.5f, Pause::FirstButtonY },
			{ Pause::ButtonWidth, Pause::ButtonHeight }) &&
		m_Impl->RetryButton.Initialize(
			"RETRY",
			{ SCREEN_WIDTH * 0.5f, Pause::FirstButtonY + Pause::ButtonGap },
			{ Pause::ButtonWidth, Pause::ButtonHeight }) &&
		m_Impl->TitleButton.Initialize(
			"TITLE",
			{ SCREEN_WIDTH * 0.5f, Pause::FirstButtonY + Pause::ButtonGap * 2.0f },
			{ Pause::ButtonWidth, Pause::ButtonHeight }) &&
		m_Impl->ExitButton.Initialize(
			"EXIT",
			{ SCREEN_WIDTH * 0.5f, Pause::FirstButtonY + Pause::ButtonGap * 3.0f },
			{ Pause::ButtonWidth, Pause::ButtonHeight });
	bool augment_buttons_ready = true;
	for (int i = 0; i < Augment::ChoiceCount; ++i)
	{
		const bool initialized = m_Impl->AugmentSelectButtons[i].Initialize(
			"SELECT",
			{ Augment::GetCardCenterX(i), Augment::SelectButtonY },
			{ Augment::SelectButtonWidth, Augment::SelectButtonHeight });
		augment_buttons_ready = initialized && augment_buttons_ready;
	}
	if (m_Impl->PanelTextureID == TEXTURE_INVALID_ID ||
		!pause_buttons_ready || !augment_buttons_ready)
	{
		Finalize();
		return false;
	}

	m_Impl->PauseTitleText = CreateCenteredText("PAUSED", 155.0f, 46.0f, 48.0f);
	m_Impl->AugmentTitleText = CreateCenteredText(
		"LEVEL UP - CHOOSE ONE", 155.0f, 30.0f, 42.0f);
	m_Impl->RoundRewardTitleText = CreateCenteredText(
		"ROUND CLEAR - CHOOSE ONE", 155.0f, 30.0f, 42.0f);
	m_Impl->AugmentHelpText = CreateCenteredText(
		"A/D OR ARROWS: CHOOSE    ENTER / CLICK: SELECT",
		875.0f,
		16.0f,
		24.0f);
	Reset();
	return true;
}

void IngameMenuController::Finalize()
{
	for (int i = 0; i < Augment::ChoiceCount; ++i)
	{
		m_Impl->AugmentDetailTexts[i].reset();
		m_Impl->AugmentWeaponTexts[i].reset();
		m_Impl->AugmentSelectButtons[i].Finalize();
	}
	m_Impl->ExitButton.Finalize();
	m_Impl->TitleButton.Finalize();
	m_Impl->RetryButton.Finalize();
	m_Impl->ResumeButton.Finalize();
	m_Impl->AugmentHelpText.reset();
	m_Impl->RoundRewardTitleText.reset();
	m_Impl->AugmentTitleText.reset();
	m_Impl->PauseTitleText.reset();
	Texture_Release(m_Impl->PanelTextureID);
	m_Impl->PanelTextureID = TEXTURE_INVALID_ID;
	m_Impl->PauseOpen = false;
	m_Impl->PauseTransitioning = false;
	m_Impl->AugmentOpen = false;
}

void IngameMenuController::Reset()
{
	m_Impl->PauseOpen = false;
	m_Impl->PauseTransitioning = false;
	m_Impl->AugmentOpen = false;
	m_Impl->SelectPauseButton(Pause::ResumeButton);
	m_Impl->SelectAugment(0);
}

void IngameMenuController::OpenPause()
{
	Button_PlayConfirmSound();
	m_Impl->PauseOpen = true;
	m_Impl->PauseTransitioning = false;
	m_Impl->SelectPauseButton(Pause::ResumeButton);
	m_Impl->LastMouseX = InputMouse_GetX();
	m_Impl->LastMouseY = InputMouse_GetY();
	InputMouse_SetVisible(true);
}

void IngameMenuController::ClosePause()
{
	m_Impl->PauseOpen = false;
	m_Impl->PauseTransitioning = false;
	InputMouse_SetVisible(false);
}

bool IngameMenuController::IsPauseOpen() const
{
	return m_Impl->PauseOpen;
}

IngamePauseAction IngameMenuController::UpdatePause()
{
	if (!m_Impl->PauseOpen || m_Impl->PauseTransitioning)
	{
		return IngamePauseAction::None;
	}
	if (InputKeyboard_IsTrigger(KK_ESCAPE))
	{
		Button_PlayBackSound();
		return IngamePauseAction::Resume;
	}

	const bool resume_clicked = m_Impl->ResumeButton.Update();
	const bool retry_clicked = m_Impl->RetryButton.Update();
	const bool title_clicked = m_Impl->TitleButton.Update();
	const bool exit_clicked = m_Impl->ExitButton.Update();
	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved =
		mouse_x != m_Impl->LastMouseX || mouse_y != m_Impl->LastMouseY;
	m_Impl->LastMouseX = mouse_x;
	m_Impl->LastMouseY = mouse_y;
	if (mouse_moved && m_Impl->ResumeButton.IsHovered())
	{
		m_Impl->SelectPauseButton(Pause::ResumeButton);
	}
	else if (mouse_moved && m_Impl->RetryButton.IsHovered())
	{
		m_Impl->SelectPauseButton(Pause::RetryButton);
	}
	else if (mouse_moved && m_Impl->TitleButton.IsHovered())
	{
		m_Impl->SelectPauseButton(Pause::TitleButton);
	}
	else if (mouse_moved && m_Impl->ExitButton.IsHovered())
	{
		m_Impl->SelectPauseButton(Pause::ExitButton);
	}

	if (InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		Button_PlayNavigateSound();
		m_Impl->SelectPauseButton(
			m_Impl->SelectedPauseButton + Pause::ButtonCount - 1);
	}
	if (InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		Button_PlayNavigateSound();
		m_Impl->SelectPauseButton(m_Impl->SelectedPauseButton + 1);
	}

	const bool keyboard_activate = InputKeyboard_IsTrigger(KK_ENTER) ||
		InputKeyboard_IsTrigger(KK_SPACE);
	bool activate = keyboard_activate;
	if (resume_clicked)
	{
		m_Impl->SelectPauseButton(Pause::ResumeButton);
		activate = true;
	}
	else if (retry_clicked)
	{
		m_Impl->SelectPauseButton(Pause::RetryButton);
		activate = true;
	}
	else if (title_clicked)
	{
		m_Impl->SelectPauseButton(Pause::TitleButton);
		activate = true;
	}
	else if (exit_clicked)
	{
		m_Impl->SelectPauseButton(Pause::ExitButton);
		activate = true;
	}
	if (!activate)
	{
		return IngamePauseAction::None;
	}
	if (keyboard_activate)
	{
		Button_PlayConfirmSound();
	}

	switch (m_Impl->SelectedPauseButton)
	{
	case Pause::ResumeButton:
		return IngamePauseAction::Resume;
	case Pause::RetryButton:
		m_Impl->PauseTransitioning = true;
		return IngamePauseAction::Retry;
	case Pause::TitleButton:
		m_Impl->PauseTransitioning = true;
		return IngamePauseAction::Title;
	case Pause::ExitButton:
		return IngamePauseAction::Exit;
	default:
		return IngamePauseAction::None;
	}
}

bool IngameMenuController::OpenAugment(IngameAugmentReason reason)
{
	m_Impl->AugmentReason = reason;
	if (!m_Impl->BuildAugmentOptions())
	{
		m_Impl->AugmentOpen = false;
		return false;
	}
	m_Impl->AugmentOpen = true;
	m_Impl->SelectAugment(0);
	m_Impl->LastMouseX = InputMouse_GetX();
	m_Impl->LastMouseY = InputMouse_GetY();
	InputMouse_SetVisible(true);
	return true;
}

void IngameMenuController::CloseAugment()
{
	m_Impl->AugmentOpen = false;
	InputMouse_SetVisible(false);
}

bool IngameMenuController::IsAugmentOpen() const
{
	return m_Impl->AugmentOpen;
}

IngameAugmentSelection IngameMenuController::UpdateAugment()
{
	if (!m_Impl->AugmentOpen)
	{
		return {};
	}

	int clicked_choice = -1;
	for (int i = 0; i < Augment::ChoiceCount; ++i)
	{
		if (m_Impl->AugmentSelectButtons[i].Update())
		{
			clicked_choice = i;
		}
	}
	const int mouse_x = InputMouse_GetX();
	const int mouse_y = InputMouse_GetY();
	const bool mouse_moved =
		mouse_x != m_Impl->LastMouseX || mouse_y != m_Impl->LastMouseY;
	m_Impl->LastMouseX = mouse_x;
	m_Impl->LastMouseY = mouse_y;
	if (mouse_moved)
	{
		for (int i = 0; i < Augment::ChoiceCount; ++i)
		{
			if (m_Impl->AugmentSelectButtons[i].IsHovered())
			{
				m_Impl->SelectAugment(i);
				break;
			}
		}
	}

	if (InputKeyboard_IsTrigger(KK_LEFT) || InputKeyboard_IsTrigger(KK_A) ||
		InputKeyboard_IsTrigger(KK_UP) || InputKeyboard_IsTrigger(KK_W))
	{
		Button_PlayNavigateSound();
		m_Impl->SelectAugment(m_Impl->SelectedAugment + Augment::ChoiceCount - 1);
	}
	else if (InputKeyboard_IsTrigger(KK_RIGHT) || InputKeyboard_IsTrigger(KK_D) ||
		InputKeyboard_IsTrigger(KK_DOWN) || InputKeyboard_IsTrigger(KK_S))
	{
		Button_PlayNavigateSound();
		m_Impl->SelectAugment(m_Impl->SelectedAugment + 1);
	}

	if (clicked_choice >= 0)
	{
		m_Impl->SelectAugment(clicked_choice);
	}
	else if (!InputKeyboard_IsTrigger(KK_ENTER))
	{
		return {};
	}
	else
	{
		Button_PlayConfirmSound();
	}

	const IngameAugmentSelection selection =
		m_Impl->AugmentOptions[m_Impl->SelectedAugment];
	return selection.Choice != IngameAugmentChoice::None ?
		selection : IngameAugmentSelection{};
}

void IngameMenuController::Draw(int overlay_texture_id)
{
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	if (m_Impl->PauseOpen)
	{
		Sprite_DrawSized(
			overlay_texture_id,
			SCREEN_WIDTH * 0.5f,
			SCREEN_HEIGHT * 0.5f,
			static_cast<float>(SCREEN_WIDTH),
			static_cast<float>(SCREEN_HEIGHT),
			{ 0.0f, 0.0f, 0.0f, 0.72f });
		DrawPanel(
			m_Impl->PanelTextureID, Pause::PanelWidth, Pause::PanelHeight);
		if (m_Impl->PauseTitleText)
		{
			m_Impl->PauseTitleText->Clear();
			m_Impl->PauseTitleText->SetText(
				"PAUSED", { 1.0f, 0.78f, 0.30f, 1.0f });
			m_Impl->PauseTitleText->Draw();
		}
		m_Impl->ResumeButton.Draw();
		m_Impl->RetryButton.Draw();
		m_Impl->TitleButton.Draw();
		m_Impl->ExitButton.Draw();
		return;
	}

	if (!m_Impl->AugmentOpen)
	{
		return;
	}
	DrawDimmer(m_Impl->PanelTextureID, 0.86f);
	DrawPanel(
		m_Impl->PanelTextureID, Augment::PanelWidth, Augment::PanelHeight);
	hal::DebugText* augment_title =
		m_Impl->AugmentReason == IngameAugmentReason::RoundClear ?
		m_Impl->RoundRewardTitleText.get() : m_Impl->AugmentTitleText.get();
	if (augment_title)
	{
		augment_title->Clear();
		augment_title->SetText(
			m_Impl->AugmentReason == IngameAugmentReason::RoundClear ?
				"ROUND CLEAR - CHOOSE ONE" : "LEVEL UP - CHOOSE ONE",
			{ 1.0f, 0.78f, 0.30f, 1.0f });
		augment_title->Draw();
	}

	for (int i = 0; i < Augment::ChoiceCount; ++i)
	{
		const float card_x = Augment::GetCardCenterX(i);
		const bool selected = m_Impl->SelectedAugment == i;
		DrawPanelAt(
			m_Impl->PanelTextureID,
			card_x,
			Augment::CardCenterY,
			Augment::CardWidth,
			Augment::CardHeight,
			selected ? DirectX::XMFLOAT4{ 1.0f, 0.83f, 0.42f, 1.0f } :
				DirectX::XMFLOAT4{ 0.72f, 0.70f, 0.74f, 0.96f });
		DrawPanelAt(
			m_Impl->PanelTextureID,
			card_x,
			Augment::IconCenterY,
			Augment::IconFrameSize,
			Augment::IconFrameSize,
			{ 0.60f, 0.55f, 0.50f, 0.96f });

		const BulletType weapon_type = m_Impl->AugmentOptions[i].WeaponType;
		const int weapon_index = static_cast<int>(weapon_type);
		if (weapon_index >= 0 && weapon_index < static_cast<int>(BulletType::Count))
		{
			const int texture_id = GameBullet::GetWeaponTextureID(weapon_type);
			const WeaponData& weapon = GameDataManager::GetInstance()
				.GetWeaponGameData().Get(static_cast<std::size_t>(weapon_index));
			const float maximum_dimension = std::max(weapon.Width, weapon.Height);
			const float icon_scale = maximum_dimension > 0.0f ?
				Augment::IconMaximumSize / maximum_dimension : 1.0f;
			if (texture_id != TEXTURE_INVALID_ID)
			{
				Sprite_DrawSized(
					texture_id,
					card_x,
					Augment::IconCenterY,
					weapon.Width * icon_scale,
					weapon.Height * icon_scale,
					{ 1.0f, 0.92f, 0.64f, 1.0f });
			}
		}

		if (m_Impl->AugmentWeaponTexts[i])
		{
			m_Impl->AugmentWeaponTexts[i]->Clear();
			m_Impl->AugmentWeaponTexts[i]->SetText(
				GetWeaponName(weapon_type), { 0.94f, 0.96f, 1.0f, 1.0f });
			m_Impl->AugmentWeaponTexts[i]->Draw();
		}
		if (m_Impl->AugmentDetailTexts[i])
		{
			m_Impl->AugmentDetailTexts[i]->Clear();
			m_Impl->AugmentDetailTexts[i]->SetText(
				GetEffectDetail(m_Impl->AugmentOptions[i].Choice),
				{ 1.0f, 0.78f, 0.30f, 1.0f });
			m_Impl->AugmentDetailTexts[i]->Draw();
		}
		m_Impl->AugmentSelectButtons[i].Draw();
	}

	if (m_Impl->AugmentHelpText)
	{
		m_Impl->AugmentHelpText->Clear();
		m_Impl->AugmentHelpText->SetText(
			"A/D OR ARROWS: CHOOSE    ENTER / CLICK: SELECT",
			{ 0.72f, 0.66f, 0.58f, 1.0f });
		m_Impl->AugmentHelpText->Draw();
	}
}
