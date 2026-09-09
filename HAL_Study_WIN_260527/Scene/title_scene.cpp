#include "title_scene.h"

#include "config.h"
#include "bitmap_text.h"
#include "direct3d.h"
#include "game_data_manager.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "math_utils.h"
#include "scene_manager.h"
#include "sprite.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace
{

	namespace TitleTuning::Credits
	{
		constexpr char Header[] = "CREDITS";
	} // namespace TitleTuning::Credits

	namespace TitleTuning::HowTo
	{
		constexpr char Header[] = "HOW TO PLAY";
	} // namespace TitleTuning::HowTo

	namespace TitleTuning::Intro
	{
		constexpr float FadeDuration = 0.48f;
	} // namespace TitleTuning::Intro

	namespace TitleTuning::Menu
	{
		constexpr int StartButtonIndex = 0;
	} // namespace TitleTuning::Menu

	namespace TitleTuning::Transition
	{
		constexpr float StartTransitionDuration = 0.62f;
	} // namespace TitleTuning::Transition

} // namespace

namespace
{

	float EaseOutBack(float value)
	{
		static constexpr float Overshoot = 1.45f;
		static constexpr float ShiftedOvershoot = Overshoot + 1.0f;

		value = Saturate(value) - 1.0f;
		return 1.0f + ShiftedOvershoot * value * value * value + Overshoot * value * value;
	}

} // namespace

TitleScene::~TitleScene() = default;

bool TitleScene::Initialize()
{
	static constexpr std::array<const wchar_t*, 4> TexturePaths = {
		L"asset/texture/map/dungeon/torch/torch_1.png",
		L"asset/texture/map/dungeon/torch/torch_2.png",
		L"asset/texture/map/dungeon/torch/torch_3.png",
		L"asset/texture/map/dungeon/torch/torch_4.png",
	};
	static constexpr float LongestLineCharacters = 45.0f;
	static constexpr float GlyphWidth = 20.0f;
	static constexpr float CharacterSpacing = 20.0f;
	static constexpr float LongestLineWidth = GlyphWidth + (LongestLineCharacters - 1.0f) * CharacterSpacing;
	static constexpr float BodyX = SCREEN_WIDTH * 0.5f - LongestLineWidth * 0.5f;
	static constexpr float BodyY = 174.0f;
	static constexpr float HeaderY = 90.0f;

	if (!GameDataManager::GetInstance().LoadAll())
	{
		return false;
	}
	m_LogoTextureID = Texture_Load(L"asset/texture/title/Logo_DungeonSurvivor.png");
	m_UiTextureID = Texture_Load(L"asset/texture/map/dungeon/ground/floor_dark.png", false);
	m_BackgroundFarTextureID = Texture_Load(L"asset/texture/title/title_bg_far_v2.png", false);
	m_BackgroundMiddleTextureID = Texture_Load(L"asset/texture/title/title_bg_middle_v2.png", false);
	m_BackgroundFrontTextureID = Texture_Load(L"asset/texture/title/title_bg_front_v2.png", false);
	m_WhiteTextureID = Texture_Load(L"asset/texture/white_square.png", false);
	for (std::size_t i = 0; i < m_TorchTextureIDs.size(); ++i)
	{
		m_TorchTextureIDs[i] = Texture_Load(TexturePaths[i], false);
	}
	if (m_LogoTextureID == TEXTURE_INVALID_ID || m_UiTextureID == TEXTURE_INVALID_ID ||
	    m_BackgroundFarTextureID == TEXTURE_INVALID_ID || m_BackgroundMiddleTextureID == TEXTURE_INVALID_ID ||
	    m_BackgroundFrontTextureID == TEXTURE_INVALID_ID || m_WhiteTextureID == TEXTURE_INVALID_ID ||
	    std::any_of(m_TorchTextureIDs.begin(), m_TorchTextureIDs.end(),
	                [](int texture_id)
	                {
		                return texture_id == TEXTURE_INVALID_ID;
	                }))
	{
		Finalize();
		return false;
	}

	m_StartButton.Initialize("START", { SCREEN_WIDTH * 0.5f, 500.0f }, { 345.0f, 78.0f });

	m_HowToButton.Initialize("HOW TO", { SCREEN_WIDTH * 0.5f, 595.0f }, { 345.0f, 78.0f });

	m_CreditsButton.Initialize("CREDITS", { SCREEN_WIDTH * 0.5f, 690.0f }, { 345.0f, 78.0f });

	m_ExitButton.Initialize("EXIT", { SCREEN_WIDTH * 0.5f, 785.0f }, { 345.0f, 78.0f });

	m_BackButton.Initialize("BACK", { SCREEN_WIDTH * 0.5f, 770.0f }, { 345.0f, 90.0f });

	m_BackButton.SetClickSound(ButtonClickSound::Back);
	m_CreditsBackButton.Initialize("BACK", { SCREEN_WIDTH * 0.5f, 970.0f }, { 345.0f, 90.0f });
	m_CreditsBackButton.SetClickSound(ButtonClickSound::Back);
	m_CreditsBackButton.SetEnabled(false);

	m_HowToHeaderText = hal::CreateCenteredText(TitleTuning::HowTo::Header, SCREEN_WIDTH * 0.5f, 245.0f, 34.0f, 32.0f);
	m_HowToBodyText = std::make_unique<hal::BitmapText>(
	    Direct3D_GetDevice(), Direct3D_GetDeviceContext(), L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
	    SCREEN_WIDTH, SCREEN_HEIGHT, 540.0f, 335.0f, 6, 0, 55.0f, 22.0f);
	m_CreditsHeaderText =
	    hal::CreateCenteredText(TitleTuning::Credits::Header, SCREEN_WIDTH * 0.5f, HeaderY, 34.0f, 32.0f);
	m_CreditsBodyText = std::make_unique<hal::BitmapText>(
	    Direct3D_GetDevice(), Direct3D_GetDeviceContext(), L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
	    SCREEN_WIDTH, SCREEN_HEIGHT, BodyX, BodyY, 24, 0, 32.0f, CharacterSpacing);

	m_MainMenu.Initialize({ &m_StartButton, &m_HowToButton, &m_CreditsButton, &m_ExitButton },
	                      TitleTuning::Menu::StartButtonIndex);
	m_ShowHowTo = false;
	m_ShowCredits = false;
	m_IsTransitioning = false;
	m_BackgroundTime = 0.0f;
	m_PresentationTime = 0.0f;
	m_TransitionTime = 0.0f;
	m_BackButton.SetEnabled(false);
	return true;
}

void TitleScene::Finalize()
{
	m_MainMenu.Clear();
	m_BackButton.Finalize();
	m_CreditsBackButton.Finalize();
	m_ExitButton.Finalize();
	m_CreditsButton.Finalize();
	m_HowToButton.Finalize();
	m_StartButton.Finalize();
	m_CreditsBodyText.reset();
	m_CreditsHeaderText.reset();
	m_HowToBodyText.reset();
	m_HowToHeaderText.reset();
	for (int& texture_id : m_TorchTextureIDs)
	{
		Texture_Release(texture_id);
		texture_id = TEXTURE_INVALID_ID;
	}
	Texture_Release(m_WhiteTextureID);
	m_WhiteTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_BackgroundFrontTextureID);
	m_BackgroundFrontTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_BackgroundMiddleTextureID);
	m_BackgroundMiddleTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_BackgroundFarTextureID);
	m_BackgroundFarTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_UiTextureID);
	m_UiTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_LogoTextureID);
	m_LogoTextureID = TEXTURE_INVALID_ID;
}

void TitleScene::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	static constexpr float InputDelay = 0.72f;
	static constexpr float TimeWrap = 240.0f;

	const float frame_time = std::clamp(delta_time, 0.0f, 0.1f);
	m_BackgroundTime = std::fmod(m_BackgroundTime + frame_time, TimeWrap);
	m_PresentationTime += frame_time;
	if (m_IsTransitioning)
	{
		m_TransitionTime += frame_time;
		if (m_TransitionTime >= TitleTuning::Transition::StartTransitionDuration)
		{
			SceneManager_ChangeScene(SceneID::Ingame);
		}
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

	if (m_PresentationTime >= InputDelay)
	{
		UpdateMainMenu();
	}
}

void TitleScene::Draw()
{
	if (m_LogoTextureID == TEXTURE_INVALID_ID || m_UiTextureID == TEXTURE_INVALID_ID ||
	    m_BackgroundFarTextureID == TEXTURE_INVALID_ID || m_BackgroundMiddleTextureID == TEXTURE_INVALID_ID ||
	    m_BackgroundFrontTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	Sprite_ResetViewMatrix();
	const bool lighting_enabled = Sprite_SetLightingEnabled(false);
	DrawBackground();
	DrawAtmosphere();

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
	DrawPresentationOverlay();
	Sprite_SetLightingEnabled(lighting_enabled);
}

void TitleScene::DrawBackground()
{
	static constexpr float LayerHeight = 1152.0f;
	static constexpr float LayerWidth = 2048.0f;

	const float center_x = SCREEN_WIDTH * 0.5f;
	const float center_y = SCREEN_HEIGHT * 0.5f;
	const float mouse_x = std::clamp((static_cast<float>(InputMouse_GetX()) - center_x) / center_x, -1.0f, 1.0f);
	const float mouse_y = std::clamp((static_cast<float>(InputMouse_GetY()) - center_y) / center_y, -1.0f, 1.0f);
	const float transition = SmoothStep(m_TransitionTime / TitleTuning::Transition::StartTransitionDuration);
	const float zoom = 1.0f + transition * 0.10f;
	const float impact_shake =
	    m_IsTransitioning ? std::sin(m_TransitionTime * 72.0f) * (1.0f - transition) * 2.5f : 0.0f;

	const bool lighting_enabled = Sprite_SetLightingEnabled(false);
	Sprite_DrawSized(m_BackgroundFarTextureID, center_x - mouse_x + impact_shake, center_y - mouse_y * 0.5f,
	                 LayerWidth * zoom, LayerHeight * zoom);
	Sprite_DrawSized(m_BackgroundMiddleTextureID,
	                 center_x - mouse_x * 5.0f + std::sin(m_BackgroundTime * 0.16f) * 2.0f + impact_shake,
	                 center_y - mouse_y * 2.0f, LayerWidth * zoom, LayerHeight * zoom);
	Sprite_DrawSized(m_BackgroundFrontTextureID,
	                 center_x - mouse_x * 12.0f + std::sin(m_BackgroundTime * 0.21f + 1.7f) * 3.0f + impact_shake,
	                 center_y - mouse_y * 5.0f, LayerWidth * zoom, LayerHeight * zoom);
	Sprite_SetLightingEnabled(lighting_enabled);
}

void TitleScene::DrawAtmosphere()
{
	static constexpr float Y = 492.0f;
	static constexpr float FrameDuration = 0.12f;

	if (m_WhiteTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const bool lighting_enabled = Sprite_SetLightingEnabled(false);
	const float intro_visibility = SmoothStep(m_PresentationTime / TitleTuning::Intro::FadeDuration);
	const float transition = SmoothStep(m_TransitionTime / TitleTuning::Transition::StartTransitionDuration);
	const float zoom = 1.0f + transition * 0.10f;
	const int torch_frame =
	    static_cast<int>(m_BackgroundTime / FrameDuration) % static_cast<int>(m_TorchTextureIDs.size());
	const float flicker =
	    0.86f + std::sin(m_BackgroundTime * 19.0f) * 0.08f + std::sin(m_BackgroundTime * 31.0f + 0.8f) * 0.04f;
	const std::array<float, 2> torch_x = { 255.0f, static_cast<float>(SCREEN_WIDTH) - 255.0f };

	for (float x : torch_x)
	{
		const float zoomed_x = SCREEN_WIDTH * 0.5f + (x - SCREEN_WIDTH * 0.5f) * zoom;
		const float zoomed_y = SCREEN_HEIGHT * 0.5f + (Y - SCREEN_HEIGHT * 0.5f) * zoom;
		Sprite_DrawSized(m_TorchTextureIDs[torch_frame], zoomed_x, zoomed_y, 64.0f * zoom, 64.0f * zoom,
		                 { 1.0f, flicker, 0.82f, intro_visibility });
	}

	// 별도 파티클 시스템 없이 일정한 패턴의 작은 불씨로 타이틀 배경을 움직인다.
	for (int i = 0; i < 18; ++i)
	{
		const int side = i & 1;
		const float seed = static_cast<float>(i) * 0.173f;
		const float speed = 0.16f + static_cast<float>(i % 5) * 0.025f;
		const float life = std::fmod(m_BackgroundTime * speed + seed, 1.0f);
		const float spread = std::sin(m_BackgroundTime * (0.9f + static_cast<float>(i % 4) * 0.17f) + seed * 17.0f);
		const float x = torch_x[side] + spread * (12.0f + static_cast<float>(i % 3) * 5.0f);
		const float y = Y - 24.0f - life * (85.0f + static_cast<float>(i % 4) * 14.0f);
		const float size = 2.0f + static_cast<float>(i % 3);
		const float alpha = intro_visibility * (1.0f - life) * (0.38f + static_cast<float>(i % 4) * 0.10f);
		Sprite_DrawSized(m_WhiteTextureID, x, y, size, size * 1.6f, { 1.0f, 0.34f + flicker * 0.22f, 0.04f, alpha });
	}
	Sprite_SetLightingEnabled(lighting_enabled);
}

void TitleScene::UpdateMainMenu()
{
	if (m_MainMenu.Update() != ButtonMenuController::NO_ACTIVATION)
	{
		ActivateSelectedButton();
	}
}

void TitleScene::UpdateHowTo()
{
	const bool back_clicked = m_BackButton.Update();
	if (back_clicked || InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE) ||
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
	const bool back_clicked = m_CreditsBackButton.Update();
	if (back_clicked || InputKeyboard_IsTrigger(KK_ENTER) || InputKeyboard_IsTrigger(KK_SPACE) ||
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
	static constexpr int ExitButtonIndex = 3;
	static constexpr int CreditsButtonIndex = 2;
	static constexpr int HowToButtonIndex = 1;

	switch (m_MainMenu.GetSelectedIndex())
	{
	case TitleTuning::Menu::StartButtonIndex:
		m_IsTransitioning = true;
		m_TransitionTime = 0.0f;
		break;
	case HowToButtonIndex:
		OpenHowTo();
		break;
	case CreditsButtonIndex:
		OpenCredits();
		break;
	case ExitButtonIndex:
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
	m_MainMenu.ResetPointerTracking();
}

void TitleScene::OpenCredits()
{
	m_StartButton.SetEnabled(false);
	m_HowToButton.SetEnabled(false);
	m_CreditsButton.SetEnabled(false);
	m_ExitButton.SetEnabled(false);
	m_CreditsBackButton.SetEnabled(true);
	m_ShowCredits = true;
	m_CreditsBackButton.SetSelected(true);
}

void TitleScene::CloseCredits()
{
	m_CreditsBackButton.SetEnabled(false);
	m_StartButton.SetEnabled(true);
	m_HowToButton.SetEnabled(true);
	m_CreditsButton.SetEnabled(true);
	m_ExitButton.SetEnabled(true);
	m_ShowCredits = false;
	m_CreditsBackButton.SetSelected(false);
	m_MainMenu.ResetPointerTracking();
}

void TitleScene::DrawMainMenu()
{
	static constexpr float IntroDuration = 0.68f;
	static constexpr float IntroDelay = 0.08f;

	const float logo_intro = EaseOutBack((m_PresentationTime - IntroDelay) / IntroDuration);
	const float logo_visibility = SmoothStep((m_PresentationTime - IntroDelay) / (IntroDuration * 0.65f));
	const float transition = SmoothStep(m_TransitionTime / TitleTuning::Transition::StartTransitionDuration);
	const float breath = 1.0f + std::sin(m_BackgroundTime * 2.15f) * 0.006f;
	const float logo_scale = 0.50f * (0.88f + logo_intro * 0.12f) * breath * (1.0f + transition * 0.10f);
	const float logo_y = 235.0f - (1.0f - logo_intro) * 58.0f;
	const float logo_width = static_cast<float>(Texture_GetWidth(m_LogoTextureID)) * logo_scale;
	const float logo_height = static_cast<float>(Texture_GetHeight(m_LogoTextureID)) * logo_scale;
	const float glow_alpha = logo_visibility * (0.055f + (std::sin(m_BackgroundTime * 2.15f) + 1.0f) * 0.025f);

	Sprite_DrawSized(m_LogoTextureID, SCREEN_WIDTH * 0.5f, logo_y + 3.0f, logo_width * 1.018f, logo_height * 1.018f,
	                 { 1.0f, 0.26f, 0.04f, glow_alpha });
	Sprite_DrawSized(m_LogoTextureID, SCREEN_WIDTH * 0.5f, logo_y, logo_width, logo_height,
	                 { 1.0f, 1.0f, 1.0f, logo_visibility });

	m_StartButton.Draw();
	m_HowToButton.Draw();
	m_CreditsButton.Draw();
	m_ExitButton.Draw();
}

void TitleScene::DrawPresentationOverlay()
{
	if (m_WhiteTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}

	const float intro_alpha = 1.0f - SmoothStep(m_PresentationTime / TitleTuning::Intro::FadeDuration);
	if (intro_alpha > 0.0f)
	{
		Sprite_DrawSized(m_WhiteTextureID, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, static_cast<float>(SCREEN_WIDTH),
		                 static_cast<float>(SCREEN_HEIGHT), { 0.0f, 0.0f, 0.0f, intro_alpha });
	}

	if (!m_IsTransitioning)
	{
		return;
	}

	const float transition = SmoothStep(m_TransitionTime / TitleTuning::Transition::StartTransitionDuration);
	const float flash = 1.0f - Saturate(std::abs(m_TransitionTime - 0.08f) / 0.08f);
	if (flash > 0.0f)
	{
		Sprite_DrawSized(m_WhiteTextureID, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, static_cast<float>(SCREEN_WIDTH),
		                 static_cast<float>(SCREEN_HEIGHT), { 1.0f, 0.20f, 0.015f, flash * 0.12f });
	}
	const float fade = SmoothStep((transition - 0.18f) / 0.82f);
	Sprite_DrawSized(m_WhiteTextureID, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, static_cast<float>(SCREEN_WIDTH),
	                 static_cast<float>(SCREEN_HEIGHT), { 0.0f, 0.0f, 0.0f, fade });
}

void TitleScene::DrawHowTo()
{
	static constexpr char Body[] = "MOVE          W A S D\n"
	                               "DASH          SPACE\n"
	                               "SKILL         Q\n"
	                               "MAP           M\n"
	                               "INTERACT      F\n"
	                               "PAUSE         ESC";

	Sprite_DrawSized(m_UiTextureID, SCREEN_WIDTH * 0.5f, 510.0f, 980.0f, 650.0f, { 0.05f, 0.04f, 0.06f, 0.96f });
	Sprite_DrawSized(m_UiTextureID, SCREEN_WIDTH * 0.5f, 205.0f, 980.0f, 5.0f, { 1.0f, 0.35f, 0.08f, 1.0f });

	if (m_HowToHeaderText)
	{
		m_HowToHeaderText->Clear();
		m_HowToHeaderText->SetText(TitleTuning::HowTo::Header, { 1.0f, 0.50f, 0.18f, 1.0f });
		m_HowToHeaderText->Draw();
	}
	if (m_HowToBodyText)
	{
		m_HowToBodyText->Clear();
		m_HowToBodyText->SetText(Body, { 0.90f, 0.88f, 0.82f, 1.0f });
		m_HowToBodyText->Draw();
	}
	m_BackButton.Draw();
}

void TitleScene::DrawCredits()
{
	static constexpr char Body[] = "GAME DESIGN AND PROGRAMMING\n"
	                               "KIM MINJUN\n"
	                               "\n"
	                               "CTHULHU BOSS ART\n"
	                               "CHIERIT\n"
	                               "chierit.itch.io/free-cthulu\n"
	                               "\n"
	                               "FOREST MUSIC\n"
	                               "FELICITOUS FOREST\n"
	                               "TOMASZ KUCZA / magory.net\n"
	                               "opengameart.org/users/magnesus\n"
	                               "\n"
	                               "CC BY 4.0 (ART AND MUSIC):\n"
	                               "https://creativecommons.org/licenses/by/4.0/\n"
	                               "\n"
	                               "UI FONT: PIXELMPLUS12-BOLD\n"
	                               "FONT AUTHORS: ITOUHIRO / M+ FONTS PROJECT\n"
	                               "LICENSE: M+ FONT LICENSE\n"
	                               "\n"
	                               "(C) 2026 KIM MINJUN";
	static constexpr float DividerY = 140.0f;
	static constexpr float PanelHeight = 1000.0f;
	static constexpr float PanelWidth = 1480.0f;
	static constexpr float PanelCenterY = SCREEN_HEIGHT * 0.5f;

	Sprite_DrawSized(m_UiTextureID, SCREEN_WIDTH * 0.5f, PanelCenterY, PanelWidth, PanelHeight,
	                 { 0.035f, 0.025f, 0.055f, 0.97f });
	Sprite_DrawSized(m_UiTextureID, SCREEN_WIDTH * 0.5f, DividerY, PanelWidth, 5.0f, { 0.58f, 0.18f, 1.0f, 1.0f });

	if (m_CreditsHeaderText)
	{
		m_CreditsHeaderText->Clear();
		m_CreditsHeaderText->SetText(TitleTuning::Credits::Header, { 0.78f, 0.48f, 1.0f, 1.0f });
		m_CreditsHeaderText->Draw();
	}
	if (m_CreditsBodyText)
	{
		m_CreditsBodyText->Clear();
		m_CreditsBodyText->SetText(Body, { 0.91f, 0.88f, 0.96f, 1.0f });
		m_CreditsBodyText->Draw();
	}
	m_CreditsBackButton.Draw();
}
