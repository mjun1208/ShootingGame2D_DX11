#include "panel_draw.h"
#include "ingame_menu_controller.h"
#include "Constants/menu_constants.h"

#include "button.h"
#include "button_menu_controller.h"
#include "config.h"
#include "bitmap_text.h"
#include "direct3d.h"
#include "game_data_manager.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "random_utils.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "weapon_data.h"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace
{

	namespace Augment
	{

		float GetCardCenterX(int index)
		{
			return SCREEN_WIDTH * 0.5f + (static_cast<float>(index) - 1.0f) *
			                                 (MenuConstants::Augment::CardWidth + MenuConstants::Augment::CardGap);
		}
	} // namespace Augment

	void DrawDimmer(int texture_id, float alpha)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}
		Sprite_DrawRegion(texture_id, { SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f },
		                  { static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) },
		                  { MenuConstants::Panel::TextureSize / 2, MenuConstants::Panel::TextureSize / 2, 1, 1 },
		                  { 0.10f, 0.06f, 0.12f, alpha });
	}

	void DrawPanelAt(int texture_id, float center_x, float center_y, float panel_width, float panel_height,
	                 const DirectX::XMFLOAT4& color = { 1.0f, 1.0f, 1.0f, 0.98f })
	{
		DrawNineSlicePanel(
		    texture_id, { center_x, center_y }, { panel_width, panel_height },
		    { MenuConstants::Panel::TextureSize, MenuConstants::Panel::SourceBorder, MenuConstants::Panel::DrawBorder },
		    color);
	}

	void DrawPanel(int texture_id, float panel_width, float panel_height)
	{
		DrawPanelAt(texture_id, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, panel_width, panel_height);
	}

	const char* GetWeaponName(BulletType type)
	{
		switch (type)
		{
		case BulletType::Fireball:
			return "FIREBALL";
		case BulletType::Lightning:
			return "LIGHTNING";
		case BulletType::Ricochet:
			return "RICOCHET";
		case BulletType::BezierHoming:
			return "MAGIC BULLET";
		case BulletType::OrbitBlade:
			return "ORBIT BLADE";
		case BulletType::Boomerang:
			return "BOOMERANG";
		case BulletType::Shotgun:
			return "SHOTGUN";
		case BulletType::MagicBlade:
			return "MAGIC BLADE";
		case BulletType::Count:
		default:
			return "NO WEAPON";
		}
	}

	const char* GetEffectDetail(IngameAugmentChoice choice)
	{
		switch (choice)
		{
		case IngameAugmentChoice::MultiShot:
			return "PROJECTILES +1";
		case IngameAugmentChoice::Overdrive:
			return "ATTACK SPEED +20%";
		case IngameAugmentChoice::Power:
			return "DAMAGE +20%";
		case IngameAugmentChoice::None:
		default:
			return "";
		}
	}
} // namespace

struct IngameMenuController::Impl
{
	int PanelTextureID{ TEXTURE_INVALID_ID };
	std::unique_ptr<hal::BitmapText> PauseTitleText;
	std::unique_ptr<hal::BitmapText> AugmentTitleText;
	std::unique_ptr<hal::BitmapText> RoundRewardTitleText;
	std::unique_ptr<hal::BitmapText> AugmentHelpText;
	cButton ResumeButton;
	cButton RetryButton;
	cButton TitleButton;
	cButton ExitButton;
	std::array<cButton, MenuConstants::Augment::ChoiceCount> AugmentSelectButtons;
	std::array<IngameAugmentSelection, MenuConstants::Augment::ChoiceCount> AugmentOptions{};
	std::array<std::unique_ptr<hal::BitmapText>, MenuConstants::Augment::ChoiceCount> AugmentWeaponTexts;
	std::array<std::unique_ptr<hal::BitmapText>, MenuConstants::Augment::ChoiceCount> AugmentDetailTexts;
	ButtonMenuController PauseMenu;
	ButtonMenuController AugmentMenu;
	bool PauseOpen{ false };
	bool PauseTransitioning{ false };
	bool AugmentOpen{ false };
	IngameAugmentReason AugmentReason{ IngameAugmentReason::LevelUp };

	bool BuildAugmentOptions()
	{
		std::vector<IngameAugmentSelection> candidates;
		for (int weapon_index = 0; weapon_index < static_cast<int>(BulletType::Count); ++weapon_index)
		{
			const BulletType weapon_type = static_cast<BulletType>(weapon_index);
			if (!GameBullet::IsWeaponOwned(weapon_type))
			{
				continue;
			}
			candidates.push_back({ weapon_type, IngameAugmentChoice::MultiShot, AugmentReason });
			candidates.push_back({ weapon_type, IngameAugmentChoice::Overdrive, AugmentReason });
			candidates.push_back({ weapon_type, IngameAugmentChoice::Power, AugmentReason });
		}
		if (candidates.empty())
		{
			return false;
		}

		std::shuffle(candidates.begin(), candidates.end(), RandomEngine());
		for (int i = 0; i < MenuConstants::Augment::ChoiceCount; ++i)
		{
			AugmentOptions[i] = i < static_cast<int>(candidates.size()) ? candidates[i] : IngameAugmentSelection{};
			const float card_x = Augment::GetCardCenterX(i);
			AugmentWeaponTexts[i] = hal::CreateCenteredText(GetWeaponName(AugmentOptions[i].WeaponType), card_x,
			                                                MenuConstants::Augment::WeaponNameY, 18.0f, 30.0f);
			AugmentDetailTexts[i] = hal::CreateCenteredText(GetEffectDetail(AugmentOptions[i].Choice), card_x,
			                                                MenuConstants::Augment::EffectDetailY, 16.0f, 28.0f);
			AugmentSelectButtons[i].SetEnabled(AugmentOptions[i].Choice != IngameAugmentChoice::None);
		}
		return true;
	}
};

IngameMenuController::IngameMenuController() : m_Impl(std::make_unique<Impl>())
{
}

IngameMenuController::~IngameMenuController() = default;

bool IngameMenuController::Initialize()
{
	Finalize();
	m_Impl->PanelTextureID = Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_button-empty.png", false);
	const bool pause_buttons_ready =
	    m_Impl->ResumeButton.Initialize("RESUME", { SCREEN_WIDTH * 0.5f, MenuConstants::Pause::FirstButtonY },
	                                    { MenuConstants::Pause::ButtonWidth, MenuConstants::Pause::ButtonHeight }) &&
	    m_Impl->RetryButton.Initialize(
	        "RETRY", { SCREEN_WIDTH * 0.5f, MenuConstants::Pause::FirstButtonY + MenuConstants::Pause::ButtonGap },
	        { MenuConstants::Pause::ButtonWidth, MenuConstants::Pause::ButtonHeight }) &&
	    m_Impl->TitleButton.Initialize(
	        "TITLE",
	        { SCREEN_WIDTH * 0.5f, MenuConstants::Pause::FirstButtonY + MenuConstants::Pause::ButtonGap * 2.0f },
	        { MenuConstants::Pause::ButtonWidth, MenuConstants::Pause::ButtonHeight }) &&
	    m_Impl->ExitButton.Initialize(
	        "EXIT",
	        { SCREEN_WIDTH * 0.5f, MenuConstants::Pause::FirstButtonY + MenuConstants::Pause::ButtonGap * 3.0f },
	        { MenuConstants::Pause::ButtonWidth, MenuConstants::Pause::ButtonHeight });
	bool augment_buttons_ready = true;
	for (int i = 0; i < MenuConstants::Augment::ChoiceCount; ++i)
	{
		const bool initialized = m_Impl->AugmentSelectButtons[i].Initialize(
		    "SELECT", { Augment::GetCardCenterX(i), MenuConstants::Augment::SelectButtonY },
		    { MenuConstants::Augment::SelectButtonWidth, MenuConstants::Augment::SelectButtonHeight });
		augment_buttons_ready = initialized && augment_buttons_ready;
	}
	if (m_Impl->PanelTextureID == TEXTURE_INVALID_ID || !pause_buttons_ready || !augment_buttons_ready)
	{
		Finalize();
		return false;
	}
	m_Impl->PauseMenu.Initialize(
	    { &m_Impl->ResumeButton, &m_Impl->RetryButton, &m_Impl->TitleButton, &m_Impl->ExitButton },
	    MenuConstants::Pause::ResumeButton);
	m_Impl->AugmentMenu.Initialize(
	    { &m_Impl->AugmentSelectButtons[0], &m_Impl->AugmentSelectButtons[1], &m_Impl->AugmentSelectButtons[2] }, 0,
	    ButtonMenuNavigation::Both, ButtonMenuBoundary::Wrap, false);

	m_Impl->PauseTitleText = hal::CreateCenteredText("PAUSED", SCREEN_WIDTH * 0.5f, 155.0f, 46.0f, 48.0f);
	m_Impl->AugmentTitleText =
	    hal::CreateCenteredText("LEVEL UP - CHOOSE ONE", SCREEN_WIDTH * 0.5f, 155.0f, 30.0f, 42.0f);
	m_Impl->RoundRewardTitleText =
	    hal::CreateCenteredText("ROUND CLEAR - CHOOSE ONE", SCREEN_WIDTH * 0.5f, 155.0f, 30.0f, 42.0f);
	m_Impl->AugmentHelpText = hal::CreateCenteredText("A/D OR ARROWS: CHOOSE    ENTER / CLICK: SELECT",
	                                                  SCREEN_WIDTH * 0.5f, 875.0f, 16.0f, 24.0f);
	Reset();
	return true;
}

void IngameMenuController::Finalize()
{
	m_Impl->AugmentMenu.Clear();
	m_Impl->PauseMenu.Clear();
	for (int i = 0; i < MenuConstants::Augment::ChoiceCount; ++i)
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
	m_Impl->PauseMenu.SetSelectedIndex(MenuConstants::Pause::ResumeButton);
	m_Impl->AugmentMenu.SetSelectedIndex(0);
}

void IngameMenuController::OpenPause()
{
	Button_PlayConfirmSound();
	m_Impl->PauseOpen = true;
	m_Impl->PauseTransitioning = false;
	m_Impl->PauseMenu.SetSelectedIndex(MenuConstants::Pause::ResumeButton);
	m_Impl->PauseMenu.ResetPointerTracking();
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

	const int activated_button = m_Impl->PauseMenu.Update();
	if (activated_button == ButtonMenuController::NO_ACTIVATION)
	{
		return IngamePauseAction::None;
	}

	switch (activated_button)
	{
	case MenuConstants::Pause::ResumeButton:
		return IngamePauseAction::Resume;
	case MenuConstants::Pause::RetryButton:
		m_Impl->PauseTransitioning = true;
		return IngamePauseAction::Retry;
	case MenuConstants::Pause::TitleButton:
		m_Impl->PauseTransitioning = true;
		return IngamePauseAction::Title;
	case MenuConstants::Pause::ExitButton:
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
	m_Impl->AugmentMenu.SetSelectedIndex(0);
	m_Impl->AugmentMenu.ResetPointerTracking();
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

	const int selected_index = m_Impl->AugmentMenu.Update();
	if (selected_index == ButtonMenuController::NO_ACTIVATION)
	{
		return {};
	}

	const IngameAugmentSelection selection = m_Impl->AugmentOptions[selected_index];
	return selection.Choice != IngameAugmentChoice::None ? selection : IngameAugmentSelection{};
}

void IngameMenuController::Draw(int overlay_texture_id)
{
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	if (m_Impl->PauseOpen)
	{
		Sprite_DrawSized(overlay_texture_id, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f,
		                 static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT),
		                 { 0.0f, 0.0f, 0.0f, 0.72f });
		DrawPanel(m_Impl->PanelTextureID, MenuConstants::Pause::PanelWidth, MenuConstants::Pause::PanelHeight);
		if (m_Impl->PauseTitleText)
		{
			m_Impl->PauseTitleText->Clear();
			m_Impl->PauseTitleText->SetText("PAUSED", { 1.0f, 0.78f, 0.30f, 1.0f });
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
	DrawPanel(m_Impl->PanelTextureID, MenuConstants::Augment::PanelWidth, MenuConstants::Augment::PanelHeight);
	hal::BitmapText* augment_title = m_Impl->AugmentReason == IngameAugmentReason::RoundClear
	                                     ? m_Impl->RoundRewardTitleText.get()
	                                     : m_Impl->AugmentTitleText.get();
	if (augment_title)
	{
		augment_title->Clear();
		augment_title->SetText(m_Impl->AugmentReason == IngameAugmentReason::RoundClear ? "ROUND CLEAR - CHOOSE ONE"
		                                                                                : "LEVEL UP - CHOOSE ONE",
		                       { 1.0f, 0.78f, 0.30f, 1.0f });
		augment_title->Draw();
	}

	for (int i = 0; i < MenuConstants::Augment::ChoiceCount; ++i)
	{
		const float card_x = Augment::GetCardCenterX(i);
		const bool selected = m_Impl->AugmentMenu.GetSelectedIndex() == i;
		DrawPanelAt(m_Impl->PanelTextureID, card_x, MenuConstants::Augment::CardCenterY,
		            MenuConstants::Augment::CardWidth, MenuConstants::Augment::CardHeight,
		            selected ? DirectX::XMFLOAT4{ 1.0f, 0.83f, 0.42f, 1.0f }
		                     : DirectX::XMFLOAT4{ 0.72f, 0.70f, 0.74f, 0.96f });
		DrawPanelAt(m_Impl->PanelTextureID, card_x, MenuConstants::Augment::IconCenterY,
		            MenuConstants::Augment::IconFrameSize, MenuConstants::Augment::IconFrameSize,
		            { 0.60f, 0.55f, 0.50f, 0.96f });

		const BulletType weapon_type = m_Impl->AugmentOptions[i].WeaponType;
		const int weapon_index = static_cast<int>(weapon_type);
		if (GameBullet::IsValidWeaponType(weapon_type))
		{
			const int texture_id = GameBullet::GetWeaponTextureID(weapon_type);
			const WeaponData& weapon = GetWeaponData(static_cast<std::size_t>(weapon_index));
			const float maximum_dimension = std::max(weapon.Width, weapon.Height);
			const float icon_scale =
			    maximum_dimension > 0.0f ? MenuConstants::Augment::IconMaximumSize / maximum_dimension : 1.0f;
			if (texture_id != TEXTURE_INVALID_ID)
			{
				Sprite_DrawSized(texture_id, card_x, MenuConstants::Augment::IconCenterY, weapon.Width * icon_scale,
				                 weapon.Height * icon_scale, { 1.0f, 0.92f, 0.64f, 1.0f });
			}
		}

		if (m_Impl->AugmentWeaponTexts[i])
		{
			m_Impl->AugmentWeaponTexts[i]->Clear();
			m_Impl->AugmentWeaponTexts[i]->SetText(GetWeaponName(weapon_type), { 0.94f, 0.96f, 1.0f, 1.0f });
			m_Impl->AugmentWeaponTexts[i]->Draw();
		}
		if (m_Impl->AugmentDetailTexts[i])
		{
			m_Impl->AugmentDetailTexts[i]->Clear();
			m_Impl->AugmentDetailTexts[i]->SetText(GetEffectDetail(m_Impl->AugmentOptions[i].Choice),
			                                       { 1.0f, 0.78f, 0.30f, 1.0f });
			m_Impl->AugmentDetailTexts[i]->Draw();
		}
		m_Impl->AugmentSelectButtons[i].Draw();
	}

	if (m_Impl->AugmentHelpText)
	{
		m_Impl->AugmentHelpText->Clear();
		m_Impl->AugmentHelpText->SetText("A/D OR ARROWS: CHOOSE    ENTER / CLICK: SELECT",
		                                 { 0.72f, 0.66f, 0.58f, 1.0f });
		m_Impl->AugmentHelpText->Draw();
	}
}
