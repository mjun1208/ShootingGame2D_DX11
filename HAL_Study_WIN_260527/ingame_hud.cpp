#include "ingame_hud.h"

#include "button.h"
#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "game_bullet.h"
#include "game_data_manager.h"
#include "game_enemy.h"
#include "game_player.h"
#include "interactable.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "procedural_map.h"
#include "sprite.h"
#include "texture.h"
#include "weapon_data.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

namespace
{
	constexpr wchar_t DEFAULT_FONT_PATH[] =
		L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png";
	constexpr wchar_t WEAPON_NAME_FONT_PATH[] =
		L"asset/font/pixelmplus/PixelMplus12-Bold_japanese.png";

	const char* Utf8Text(const char8_t* text)
	{
		return reinterpret_cast<const char*>(text);
	}

	namespace Crosshair
	{
		constexpr float DrawSize = 72.0f;
	}

	namespace InteractionPrompt
	{
		constexpr float DrawSize = 52.0f;
		constexpr float BobAmount = 5.0f;
		constexpr float BobSpeed = 6.0f;
	}

	namespace Hud
	{
		struct AtlasRect
		{
			int X;
			int Y;
			int Width;
			int Height;
		};

		constexpr float Scale = 3.0f;
		constexpr float Margin = 28.0f;
		constexpr AtlasRect FrameSource{ 58, 60, 145, 16 };
		constexpr int FrameLeftCapWidth = 12;
		constexpr int FrameRightCapWidth = 11;
		constexpr int FrameRightCapX =
			FrameSource.X + FrameSource.Width - FrameRightCapWidth;
		constexpr int FrameCenterSourceX = FrameSource.X + FrameLeftCapWidth;
		constexpr float BarGap = 8.0f;
		constexpr AtlasRect HealthFillSource{ 70, 44, 112, 15 };
		constexpr AtlasRect ExperienceFillSource{ 58, 77, 59, 12 };
		constexpr float LevelTextGap = 18.0f;
		constexpr float ValueCharacterSpacing = 18.0f;
		constexpr int ValueCharacterCount = 9;
	}

	namespace BossHud
	{
		constexpr float Width = 1040.0f;
		constexpr float FrameHeight = 58.0f;
		constexpr float BottomMargin = 34.0f;
		constexpr float NameGap = 18.0f;
		constexpr float NameCharacterSpacing = 30.0f;
		constexpr float ValueCharacterSpacing = 20.0f;
		constexpr int ValueCharacterCount = 9;
	}

	namespace RoundHud
	{
		constexpr float MinimapViewportFraction = 0.40f;
		constexpr float MinimapMaximumWorldScale = 1.0f / 20.0f;
		constexpr float MinimapScreenMargin = 20.0f;
		constexpr float MinimapFramePadding = 8.0f;
		constexpr float GlyphWidth = 20.0f;
		constexpr float GlyphHeight = 32.0f;
		constexpr float CharacterSpacing = 18.0f;
		constexpr float TopInset = 10.0f;
		constexpr float ShadowOffset = 2.0f;
	}

	namespace QSkillHud
	{
		constexpr float CooldownDuration = 8.0f;
		constexpr float SlotSize = 88.0f;
		constexpr float InnerSize = 60.0f;
		constexpr float IconSize = 62.0f;
		constexpr float HudGap = 16.0f;
		constexpr float CenterX = Hud::Margin + SlotSize * 0.5f;
		constexpr float CenterY =
			Hud::Margin + Hud::FrameSource.Height * Hud::Scale * 2.0f +
			Hud::BarGap + HudGap + SlotSize * 0.5f;
		constexpr int PanelTextureSize = 24;
		constexpr int PanelCenterSource = PanelTextureSize / 2;
	}

	namespace WeaponUnlockHud
	{
		constexpr float PanelWidth = 720.0f;
		constexpr float PanelHeight = 470.0f;
		constexpr float PanelCenterY = SCREEN_HEIGHT * 0.5f;
		constexpr float IconCenterX = SCREEN_WIDTH * 0.5f;
		constexpr float IconCenterY = PanelCenterY - 42.0f;
		constexpr float IconFrameWidth = 144.0f;
		constexpr float IconFrameHeight = 148.0f;
		constexpr float IconMaximumSize = 100.0f;
		constexpr float OkButtonWidth = 240.0f;
		constexpr float OkButtonHeight = 68.0f;
		constexpr float OkButtonCenterY = PanelCenterY + 164.0f;
		constexpr std::size_t WeaponCount =
			static_cast<std::size_t>(BulletType::Count);
	}

	namespace DarkRpgPanel
	{
		constexpr int TextureSize = 24;
		constexpr int SourceBorder = 4;
		constexpr float DrawBorder = 18.0f;
	}

	const char* GetWeaponDisplayName(BulletType type)
	{
		switch (type)
		{
		case BulletType::Fireball: return Utf8Text(u8"\u30D5\u30A1\u30A4\u30A2\u30DC\u30FC\u30EB");
		case BulletType::Lightning: return Utf8Text(u8"\u30E9\u30A4\u30C8\u30CB\u30F3\u30B0");
		case BulletType::Ricochet: return Utf8Text(u8"\u30EA\u30B3\u30B7\u30A7\u30C3\u30C8");
		case BulletType::BezierHoming: return Utf8Text(u8"\u30DE\u30B8\u30C3\u30AF\u30D0\u30EC\u30C3\u30C8");
		case BulletType::OrbitBlade: return Utf8Text(u8"\u30AA\u30FC\u30D3\u30C3\u30C8\u30D6\u30EC\u30FC\u30C9");
		case BulletType::Boomerang: return Utf8Text(u8"\u30D6\u30FC\u30E1\u30E9\u30F3");
		case BulletType::Shotgun: return Utf8Text(u8"\u30B7\u30E7\u30C3\u30C8\u30AC\u30F3");
		case BulletType::MagicBlade: return Utf8Text(u8"\u30DE\u30B8\u30C3\u30AF\u30D6\u30EC\u30FC\u30C9");
		case BulletType::Count:
		default: return "UNKNOWN WEAPON";
		}
	}

	void DrawDarkRpgPanel(
		int texture_id,
		float center_x,
		float center_y,
		float width,
		float height,
		const DirectX::XMFLOAT4& color)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}
		const float left = center_x - width * 0.5f;
		const float top = center_y - height * 0.5f;
		const std::array<float, 3> draw_widths = {
			DarkRpgPanel::DrawBorder,
			width - DarkRpgPanel::DrawBorder * 2.0f,
			DarkRpgPanel::DrawBorder,
		};
		const std::array<float, 3> draw_heights = {
			DarkRpgPanel::DrawBorder,
			height - DarkRpgPanel::DrawBorder * 2.0f,
			DarkRpgPanel::DrawBorder,
		};
		const std::array<int, 3> source_positions = {
			0,
			DarkRpgPanel::SourceBorder,
			DarkRpgPanel::TextureSize - DarkRpgPanel::SourceBorder,
		};
		const std::array<int, 3> source_sizes = {
			DarkRpgPanel::SourceBorder,
			DarkRpgPanel::TextureSize - DarkRpgPanel::SourceBorder * 2,
			DarkRpgPanel::SourceBorder,
		};

		float draw_y = top;
		for (int row = 0; row < 3; ++row)
		{
			float draw_x = left;
			for (int column = 0; column < 3; ++column)
			{
				Sprite_DrawRegion(
					texture_id,
					draw_x + draw_widths[column] * 0.5f,
					draw_y + draw_heights[row] * 0.5f,
					draw_widths[column],
					draw_heights[row],
					source_positions[column],
					source_positions[row],
					source_sizes[column],
					source_sizes[row],
					color);
				draw_x += draw_widths[column];
			}
			draw_y += draw_heights[row];
		}
	}

	std::unique_ptr<hal::DebugText> CreateCenteredText(
		const char* text,
		float y,
		float character_spacing,
		const wchar_t* font_path = DEFAULT_FONT_PATH,
		float glyph_width = 20.0f)
	{
		const std::size_t character_count =
			hal::DebugText::CountUtf8Characters(text);
		const float text_width = character_count > 0 ?
			glyph_width +
				(static_cast<float>(character_count) - 1.0f) * character_spacing :
			0.0f;
		return std::make_unique<hal::DebugText>(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			font_path,
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			SCREEN_WIDTH * 0.5f - text_width * 0.5f,
			y,
			1,
			0,
			32.0f,
			character_spacing);
	}

	void DrawHudFrame(int texture_id, float center_y)
	{
		const float frame_width = Hud::FrameSource.Width * Hud::Scale;
		const float frame_height = Hud::FrameSource.Height * Hud::Scale;
		const float left_cap_width = Hud::FrameLeftCapWidth * Hud::Scale;
		const float right_cap_width = Hud::FrameRightCapWidth * Hud::Scale;
		const float center_width = frame_width - left_cap_width - right_cap_width;
		const DirectX::XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
		Sprite_DrawRegion(
			texture_id,
			Hud::Margin + left_cap_width * 0.5f,
			center_y,
			left_cap_width,
			frame_height,
			Hud::FrameSource.X,
			Hud::FrameSource.Y,
			Hud::FrameLeftCapWidth,
			Hud::FrameSource.Height,
			white);
		Sprite_DrawRegion(
			texture_id,
			Hud::Margin + left_cap_width + center_width * 0.5f,
			center_y,
			center_width,
			frame_height,
			Hud::FrameCenterSourceX,
			Hud::FrameSource.Y,
			1,
			Hud::FrameSource.Height,
			white);
		Sprite_DrawRegion(
			texture_id,
			Hud::Margin + left_cap_width + center_width + right_cap_width * 0.5f,
			center_y,
			right_cap_width,
			frame_height,
			Hud::FrameRightCapX,
			Hud::FrameSource.Y,
			Hud::FrameRightCapWidth,
			Hud::FrameSource.Height,
			white);
	}

	void DrawHudFill(
		int texture_id,
		float center_y,
		float ratio,
		const Hud::AtlasRect& source)
	{
		ratio = std::clamp(ratio, 0.0f, 1.0f);
		const int clipped_source_width = static_cast<int>(
			source.Width * ratio + 0.5f);
		if (clipped_source_width <= 0)
		{
			return;
		}
		const float frame_width = Hud::FrameSource.Width * Hud::Scale;
		const float left_cap_width = Hud::FrameLeftCapWidth * Hud::Scale;
		const float right_cap_width = Hud::FrameRightCapWidth * Hud::Scale;
		const float interior_width = frame_width - left_cap_width - right_cap_width;
		const float filled_draw_width = interior_width * ratio;
		Sprite_DrawRegion(
			texture_id,
			Hud::Margin + left_cap_width + filled_draw_width * 0.5f,
			center_y,
			filled_draw_width,
			10.0f * Hud::Scale,
			source.X,
			source.Y,
			clipped_source_width,
			source.Height,
			{ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void DrawPlayerBars(int texture_id)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}
		const float frame_height = Hud::FrameSource.Height * Hud::Scale;
		const float health_center_y = Hud::Margin + frame_height * 0.5f;
		const float experience_center_y =
			health_center_y + frame_height + Hud::BarGap;
		const float max_hit_point = GamePlayer::GetMaxHitPoint();
		const float health_ratio = max_hit_point > 0.0f ?
			std::clamp(GamePlayer::GetHitPoint() / max_hit_point, 0.0f, 1.0f) :
			0.0f;
		const int required_experience = GamePlayer::GetExperienceToNextLevel();
		const float experience_ratio = required_experience > 0 ?
			std::clamp(
				static_cast<float>(GamePlayer::GetExperience()) /
					static_cast<float>(required_experience),
				0.0f,
				1.0f) :
			0.0f;
		DrawHudFrame(texture_id, health_center_y);
		DrawHudFill(texture_id, health_center_y, health_ratio, Hud::HealthFillSource);
		DrawHudFrame(texture_id, experience_center_y);
		DrawHudFill(
			texture_id, experience_center_y, experience_ratio,
			Hud::ExperienceFillSource);
	}

	void DrawBossBar(int texture_id, float health_ratio)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}
		health_ratio = std::clamp(health_ratio, 0.0f, 1.0f);
		const float left = SCREEN_WIDTH * 0.5f - BossHud::Width * 0.5f;
		const float center_y = SCREEN_HEIGHT - BossHud::BottomMargin -
			BossHud::FrameHeight * 0.5f;
		const float scale = BossHud::FrameHeight / Hud::FrameSource.Height;
		const float left_cap_width = Hud::FrameLeftCapWidth * scale;
		const float right_cap_width = Hud::FrameRightCapWidth * scale;
		const float center_width = BossHud::Width - left_cap_width - right_cap_width;
		const DirectX::XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
		Sprite_DrawRegion(
			texture_id, left + left_cap_width * 0.5f, center_y,
			left_cap_width, BossHud::FrameHeight,
			Hud::FrameSource.X, Hud::FrameSource.Y,
			Hud::FrameLeftCapWidth, Hud::FrameSource.Height, white);
		Sprite_DrawRegion(
			texture_id, left + left_cap_width + center_width * 0.5f, center_y,
			center_width, BossHud::FrameHeight,
			Hud::FrameCenterSourceX, Hud::FrameSource.Y,
			1, Hud::FrameSource.Height, white);
		Sprite_DrawRegion(
			texture_id,
			left + left_cap_width + center_width + right_cap_width * 0.5f,
			center_y,
			right_cap_width,
			BossHud::FrameHeight,
			Hud::FrameRightCapX,
			Hud::FrameSource.Y,
			Hud::FrameRightCapWidth,
			Hud::FrameSource.Height,
			white);

		const float fill_width = (center_width + 4.0f) * health_ratio;
		if (fill_width <= 0.0f)
		{
			return;
		}
		const int source_width = std::max(
			1,
			static_cast<int>(Hud::HealthFillSource.Width * health_ratio + 0.5f));
		const DirectX::XMFLOAT4 phase_color = health_ratio <= 0.5f ?
			DirectX::XMFLOAT4{ 1.0f, 0.52f, 0.42f, 1.0f } :
			DirectX::XMFLOAT4{ 1.0f, 0.90f, 0.90f, 1.0f };
		Sprite_DrawRegion(
			texture_id,
			left + left_cap_width + fill_width * 0.5f,
			center_y,
			fill_width,
			BossHud::FrameHeight - 18.0f,
			Hud::HealthFillSource.X,
			Hud::HealthFillSource.Y,
			source_width,
			Hud::HealthFillSource.Height,
			phase_color);
	}
}

struct IngameHud::Impl
{
	int CrosshairTextureID{ TEXTURE_INVALID_ID };
	int PlayerHudTextureID{ TEXTURE_INVALID_ID };
	int SkillPanelTextureID{ TEXTURE_INVALID_ID };
	int QSkillIconTextureID{ TEXTURE_INVALID_ID };
	int InteractionPromptTextureID{ TEXTURE_INVALID_ID };
	std::array<int, WeaponUnlockHud::WeaponCount> WeaponIconTextureIDs{};
	BulletType UnlockedWeaponType{ BulletType::Count };
	cButton WeaponUnlockOkButton;
	bool WeaponUnlockPopupOpen{ false };
	std::unique_ptr<hal::DebugText> HealthValueText;
	std::unique_ptr<hal::DebugText> ExperienceValueText;
	std::unique_ptr<hal::DebugText> PlayerLevelText;
	std::unique_ptr<hal::DebugText> QSkillKeyText;
	std::unique_ptr<hal::DebugText> QSkillCooldownText;
	std::unique_ptr<hal::DebugText> RoundShadowText;
	std::unique_ptr<hal::DebugText> RoundText;
	std::unique_ptr<hal::DebugText> TimeSlashText;
	std::unique_ptr<hal::DebugText> BossNameText;
	std::unique_ptr<hal::DebugText> BossHealthText;
	std::unique_ptr<hal::DebugText> WeaponUnlockTitleText;
	std::unique_ptr<hal::DebugText> WeaponUnlockNameText;
};

IngameHud::IngameHud()
	: m_Impl(std::make_unique<Impl>())
{
}

IngameHud::~IngameHud() = default;

bool IngameHud::Initialize(const DirectX::XMFLOAT2& viewport_size)
{
	Finalize();
	m_Impl->CrosshairTextureID = Texture_Load(
		L"asset/texture/crosshair.png", false);
	m_Impl->PlayerHudTextureID = Texture_Load(
		L"asset/dark_rpg_gui/dfgui_partyhud.png", false);
	m_Impl->SkillPanelTextureID = Texture_Load(
		L"asset/dark_rpg_gui/dfgui_button-empty.png", false);
	m_Impl->QSkillIconTextureID = Texture_Load(
		L"asset/texture/ui/skill/q_time_slash.png", false);
	m_Impl->InteractionPromptTextureID = Texture_Load(
		L"asset/texture/ui/input/key-f.png", false);
	const bool weapon_unlock_button_ready =
		m_Impl->WeaponUnlockOkButton.Initialize(
			"OK",
			{ SCREEN_WIDTH * 0.5f, WeaponUnlockHud::OkButtonCenterY },
			{ WeaponUnlockHud::OkButtonWidth, WeaponUnlockHud::OkButtonHeight });
	m_Impl->WeaponIconTextureIDs.fill(TEXTURE_INVALID_ID);
	for (std::size_t i = 0; i < WeaponUnlockHud::WeaponCount; ++i)
	{
		m_Impl->WeaponIconTextureIDs[i] =
			GameBullet::GetWeaponTextureID(static_cast<BulletType>(i));
	}
	if (m_Impl->CrosshairTextureID == TEXTURE_INVALID_ID ||
		m_Impl->PlayerHudTextureID == TEXTURE_INVALID_ID ||
		m_Impl->SkillPanelTextureID == TEXTURE_INVALID_ID ||
		m_Impl->QSkillIconTextureID == TEXTURE_INVALID_ID ||
		!weapon_unlock_button_ready)
	{
		Finalize();
		return false;
	}

	const float hud_frame_width = Hud::FrameSource.Width * Hud::Scale;
	const float hud_frame_height = Hud::FrameSource.Height * Hud::Scale;
	const float health_center_y = Hud::Margin + hud_frame_height * 0.5f;
	const float experience_center_y =
		Hud::Margin + hud_frame_height * 1.5f + Hud::BarGap;
	const float hud_value_width = 20.0f +
		(Hud::ValueCharacterCount - 1) * Hud::ValueCharacterSpacing;
	const float hud_value_x =
		Hud::Margin + hud_frame_width * 0.5f - hud_value_width * 0.5f;
	const wchar_t* font_path = DEFAULT_FONT_PATH;
	m_Impl->HealthValueText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT, hud_value_x, health_center_y - 16.0f,
		1, 0, 32.0f, Hud::ValueCharacterSpacing);
	m_Impl->ExperienceValueText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT, hud_value_x, experience_center_y - 16.0f,
		1, 0, 32.0f, Hud::ValueCharacterSpacing);
	m_Impl->PlayerLevelText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		Hud::Margin + hud_frame_width + Hud::LevelTextGap,
		experience_center_y - 16.0f, 1, 0, 32.0f, 20.0f);
	m_Impl->QSkillKeyText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		QSkillHud::CenterX + 18.0f, QSkillHud::CenterY + 17.0f,
		1, 1, 20.0f, 18.0f);
	m_Impl->QSkillCooldownText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		QSkillHud::CenterX - 10.0f, QSkillHud::CenterY - 16.0f,
		1, 1, 32.0f, 20.0f);

	const int total_round_count =
		GameDataManager::GetInstance().GetMapGameData().GetTotalRoundCount();
	const std::string widest_round_text =
		"ROUND " + std::to_string(total_round_count) +
		" / " + std::to_string(total_round_count);
	const float round_text_width = RoundHud::GlyphWidth +
		(static_cast<float>(widest_round_text.size()) - 1.0f) *
		RoundHud::CharacterSpacing;
	const DirectX::XMFLOAT2 world_size = ProceduralMap_GetWorldSize();
	const float minimap_scale = std::min({
		RoundHud::MinimapMaximumWorldScale,
		std::max(viewport_size.x, 1.0f) * RoundHud::MinimapViewportFraction /
			std::max(world_size.x, 1.0f),
		std::max(viewport_size.y, 1.0f) * RoundHud::MinimapViewportFraction /
			std::max(world_size.y, 1.0f),
	});
	const float minimap_width = world_size.x * minimap_scale;
	const float minimap_origin_x = viewport_size.x - minimap_width -
		RoundHud::MinimapScreenMargin - RoundHud::MinimapFramePadding;
	const float round_text_x = minimap_origin_x +
		(minimap_width - round_text_width) * 0.5f;
	const float round_text_y = RoundHud::MinimapScreenMargin +
		RoundHud::MinimapFramePadding + RoundHud::TopInset;
	m_Impl->RoundShadowText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		round_text_x + RoundHud::ShadowOffset,
		round_text_y + RoundHud::ShadowOffset,
		1, 0, RoundHud::GlyphHeight, RoundHud::CharacterSpacing);
	m_Impl->RoundText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT, round_text_x, round_text_y,
		1, 0, RoundHud::GlyphHeight, RoundHud::CharacterSpacing);
	m_Impl->TimeSlashText = CreateCenteredText(
		"TIME SLASH 3 / 3", 52.0f, 20.0f);
	m_Impl->WeaponUnlockTitleText = CreateCenteredText(
		"WEAPON UNLOCKED", WeaponUnlockHud::PanelCenterY - 190.0f, 25.0f);
	const float boss_bar_center_y = SCREEN_HEIGHT - BossHud::BottomMargin -
		BossHud::FrameHeight * 0.5f;
	const float boss_value_width = 20.0f +
		(BossHud::ValueCharacterCount - 1) * BossHud::ValueCharacterSpacing;
	m_Impl->BossHealthText = std::make_unique<hal::DebugText>(
		Direct3D_GetDevice(), Direct3D_GetDeviceContext(), font_path,
		SCREEN_WIDTH, SCREEN_HEIGHT,
		SCREEN_WIDTH * 0.5f - boss_value_width * 0.5f,
		boss_bar_center_y - 16.0f,
		1, 0, 32.0f, BossHud::ValueCharacterSpacing);
	ResetBossPresentation();
	return true;
}

void IngameHud::Finalize()
{
	m_Impl->WeaponUnlockOkButton.Finalize();
	m_Impl->WeaponUnlockNameText.reset();
	m_Impl->WeaponUnlockTitleText.reset();
	m_Impl->BossHealthText.reset();
	m_Impl->BossNameText.reset();
	m_Impl->TimeSlashText.reset();
	m_Impl->RoundText.reset();
	m_Impl->RoundShadowText.reset();
	m_Impl->QSkillCooldownText.reset();
	m_Impl->QSkillKeyText.reset();
	m_Impl->PlayerLevelText.reset();
	m_Impl->ExperienceValueText.reset();
	m_Impl->HealthValueText.reset();
	m_Impl->WeaponIconTextureIDs.fill(TEXTURE_INVALID_ID);
	Texture_Release(m_Impl->InteractionPromptTextureID);
	Texture_Release(m_Impl->QSkillIconTextureID);
	Texture_Release(m_Impl->SkillPanelTextureID);
	Texture_Release(m_Impl->PlayerHudTextureID);
	Texture_Release(m_Impl->CrosshairTextureID);
	m_Impl->InteractionPromptTextureID = TEXTURE_INVALID_ID;
	m_Impl->QSkillIconTextureID = TEXTURE_INVALID_ID;
	m_Impl->SkillPanelTextureID = TEXTURE_INVALID_ID;
	m_Impl->PlayerHudTextureID = TEXTURE_INVALID_ID;
	m_Impl->CrosshairTextureID = TEXTURE_INVALID_ID;
	m_Impl->UnlockedWeaponType = BulletType::Count;
	m_Impl->WeaponUnlockPopupOpen = false;
}

void IngameHud::ShowWeaponUnlock(BulletType type)
{
	const int type_index = static_cast<int>(type);
	if (type_index < 0 ||
		type_index >= static_cast<int>(WeaponUnlockHud::WeaponCount))
	{
		return;
	}
	m_Impl->UnlockedWeaponType = type;
	m_Impl->WeaponUnlockPopupOpen = true;
	m_Impl->WeaponUnlockOkButton.SetSelected(true);
	m_Impl->WeaponUnlockNameText = CreateCenteredText(
		GetWeaponDisplayName(type),
		WeaponUnlockHud::PanelCenterY + 62.0f,
		28.0f,
		WEAPON_NAME_FONT_PATH,
		24.0f);
	InputMouse_SetVisible(true);
}

bool IngameHud::IsWeaponUnlockPopupOpen() const
{
	return m_Impl->WeaponUnlockPopupOpen;
}

void IngameHud::UpdateWeaponUnlockPopup()
{
	if (!m_Impl->WeaponUnlockPopupOpen)
	{
		return;
	}
	const bool button_confirmed = m_Impl->WeaponUnlockOkButton.Update();
	const bool keyboard_confirmed = InputKeyboard_IsTrigger(KK_ENTER) ||
		InputKeyboard_IsTrigger(KK_SPACE);
	const bool confirmed = button_confirmed || keyboard_confirmed;
	if (!confirmed)
	{
		return;
	}
	if (keyboard_confirmed)
	{
		Button_PlayConfirmSound();
	}
	m_Impl->WeaponUnlockPopupOpen = false;
	m_Impl->WeaponUnlockOkButton.SetSelected(false);
	InputMouse_SetVisible(false);
}

void IngameHud::ResetBossPresentation()
{
	const float boss_bar_center_y = SCREEN_HEIGHT - BossHud::BottomMargin -
		BossHud::FrameHeight * 0.5f;
	const float boss_name_y = boss_bar_center_y -
		BossHud::FrameHeight * 0.5f - BossHud::NameGap - 32.0f;
	m_Impl->BossNameText = CreateCenteredText(
		GameEnemy::GetBossDisplayName(),
		boss_name_y,
		BossHud::NameCharacterSpacing);
}

void IngameHud::Draw(
	float q_skill_cooldown_remaining,
	bool show_world_map,
	bool boss_intro_active,
	bool has_auto_aim_target,
	const DirectX::XMFLOAT2& auto_aim_screen_position)
{
	if (!show_world_map && m_Impl->RoundShadowText && m_Impl->RoundText)
	{
		const int total_round_count =
			GameDataManager::GetInstance().GetMapGameData().GetTotalRoundCount();
		const int round_digit_width = static_cast<int>(
			std::to_string(total_round_count).size());
		char round_text[32]{};
		std::snprintf(
			round_text,
			sizeof(round_text),
			"ROUND %*d / %d",
			round_digit_width,
			ProceduralMap_GetRound(),
			total_round_count);
		m_Impl->RoundShadowText->Clear();
		m_Impl->RoundShadowText->SetText(
			round_text, { 0.02f, 0.03f, 0.04f, 0.92f });
		m_Impl->RoundShadowText->Draw();
		m_Impl->RoundText->Clear();
		m_Impl->RoundText->SetText(
			round_text, { 1.0f, 0.78f, 0.30f, 1.0f });
		m_Impl->RoundText->Draw();
	}

	DrawPlayerBars(m_Impl->PlayerHudTextureID);
	const bool cooling_down = q_skill_cooldown_remaining > 0.0f;
	const DirectX::XMFLOAT4 frame_color = cooling_down ?
		DirectX::XMFLOAT4{ 0.58f, 0.62f, 0.70f, 1.0f } :
		DirectX::XMFLOAT4{ 0.52f, 0.38f, 1.0f, 1.0f };
	Sprite_DrawSized(
		m_Impl->SkillPanelTextureID,
		QSkillHud::CenterX,
		QSkillHud::CenterY,
		QSkillHud::SlotSize,
		QSkillHud::SlotSize,
		frame_color);
	Sprite_DrawSized(
		m_Impl->QSkillIconTextureID,
		QSkillHud::CenterX,
		QSkillHud::CenterY - 2.0f,
		QSkillHud::IconSize,
		QSkillHud::IconSize,
		cooling_down ?
			DirectX::XMFLOAT4{ 0.48f, 0.52f, 0.66f, 0.72f } :
			DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
	if (cooling_down)
	{
		const float cooldown_ratio = std::clamp(
			q_skill_cooldown_remaining / QSkillHud::CooldownDuration,
			0.0f,
			1.0f);
		const float overlay_height = QSkillHud::InnerSize * cooldown_ratio;
		const float inner_top =
			QSkillHud::CenterY - QSkillHud::InnerSize * 0.5f;
		Sprite_DrawRegion(
			m_Impl->SkillPanelTextureID,
			QSkillHud::CenterX,
			inner_top + overlay_height * 0.5f,
			QSkillHud::InnerSize,
			overlay_height,
			QSkillHud::PanelCenterSource,
			QSkillHud::PanelCenterSource,
			1,
			1,
			{ 0.04f, 0.05f, 0.08f, 0.82f });
		if (m_Impl->QSkillCooldownText)
		{
			char seconds_text[8]{};
			std::snprintf(
				seconds_text,
				sizeof(seconds_text),
				"%d",
				static_cast<int>(std::ceil(q_skill_cooldown_remaining)));
			m_Impl->QSkillCooldownText->Clear();
			m_Impl->QSkillCooldownText->SetText(
				seconds_text, { 0.94f, 0.96f, 1.0f, 1.0f });
			m_Impl->QSkillCooldownText->Draw();
		}
	}
	if (m_Impl->QSkillKeyText)
	{
		m_Impl->QSkillKeyText->Clear();
		m_Impl->QSkillKeyText->SetText(
			"Q",
			cooling_down ?
				DirectX::XMFLOAT4{ 0.66f, 0.70f, 0.78f, 1.0f } :
				DirectX::XMFLOAT4{ 0.34f, 0.86f, 1.0f, 1.0f });
		m_Impl->QSkillKeyText->Draw();
	}

	float boss_hit_point = 0.0f;
	float boss_max_hit_point = 0.0f;
	if (!boss_intro_active &&
		GameEnemy::TryGetBossHealth(boss_hit_point, boss_max_hit_point))
	{
		const float boss_health_ratio = boss_max_hit_point > 0.0f ?
			std::clamp(boss_hit_point / boss_max_hit_point, 0.0f, 1.0f) :
			0.0f;
		DrawBossBar(m_Impl->PlayerHudTextureID, boss_health_ratio);
		if (m_Impl->BossNameText)
		{
			m_Impl->BossNameText->Clear();
			m_Impl->BossNameText->SetText(
				GameEnemy::GetBossDisplayName(),
				boss_health_ratio <= 0.5f ?
					DirectX::XMFLOAT4{ 1.0f, 0.38f, 0.30f, 1.0f } :
					DirectX::XMFLOAT4{ 1.0f, 0.82f, 0.72f, 1.0f });
			m_Impl->BossNameText->Draw();
		}
		if (m_Impl->BossHealthText)
		{
			char boss_health_text[32]{};
			std::snprintf(
				boss_health_text,
				sizeof(boss_health_text),
				"%3d / %3d",
				static_cast<int>(std::ceil(boss_hit_point)),
				static_cast<int>(std::ceil(boss_max_hit_point)));
			m_Impl->BossHealthText->Clear();
			m_Impl->BossHealthText->SetText(
				boss_health_text, { 1.0f, 0.96f, 0.90f, 1.0f });
			m_Impl->BossHealthText->Draw();
		}
	}

	if (m_Impl->HealthValueText)
	{
		char health_text[32]{};
		std::snprintf(
			health_text,
			sizeof(health_text),
			"%3d / %3d",
			static_cast<int>(std::ceil(GamePlayer::GetHitPoint())),
			static_cast<int>(std::ceil(GamePlayer::GetMaxHitPoint())));
		m_Impl->HealthValueText->Clear();
		m_Impl->HealthValueText->SetText(
			health_text, { 1.0f, 0.92f, 0.82f, 1.0f });
		m_Impl->HealthValueText->Draw();
	}
	if (m_Impl->ExperienceValueText)
	{
		char experience_text[32]{};
		std::snprintf(
			experience_text,
			sizeof(experience_text),
			"%3d / %3d",
			GamePlayer::GetExperience(),
			GamePlayer::GetExperienceToNextLevel());
		m_Impl->ExperienceValueText->Clear();
		m_Impl->ExperienceValueText->SetText(
			experience_text, { 1.0f, 0.92f, 0.82f, 1.0f });
		m_Impl->ExperienceValueText->Draw();
	}
	if (m_Impl->PlayerLevelText)
	{
		const std::string level_text =
			std::to_string(GamePlayer::GetLevel()) + "Lv";
		m_Impl->PlayerLevelText->Clear();
		m_Impl->PlayerLevelText->SetText(
			level_text.c_str(), { 0.94f, 0.70f, 0.28f, 1.0f });
		m_Impl->PlayerLevelText->Draw();
	}
	if (m_Impl->TimeSlashText && GamePlayer::IsEmpoweredDashModeActive())
	{
		char time_slash_text[32]{};
		std::snprintf(
			time_slash_text,
			sizeof(time_slash_text),
			"TIME SLASH %d / 3",
			GamePlayer::GetEmpoweredDashCharges());
		m_Impl->TimeSlashText->Clear();
		m_Impl->TimeSlashText->SetText(
			time_slash_text, { 0.48f, 0.84f, 1.0f, 1.0f });
		m_Impl->TimeSlashText->Draw();
	}

	if (m_Impl->WeaponUnlockPopupOpen &&
		m_Impl->UnlockedWeaponType != BulletType::Count)
	{
		constexpr float alpha = 1.0f;
		Sprite_DrawRegion(
			m_Impl->SkillPanelTextureID,
			SCREEN_WIDTH * 0.5f,
			SCREEN_HEIGHT * 0.5f,
			static_cast<float>(SCREEN_WIDTH),
			static_cast<float>(SCREEN_HEIGHT),
			12,
			12,
			1,
			1,
			{ 0.0f, 0.0f, 0.0f, 0.76f });
		DrawDarkRpgPanel(
			m_Impl->SkillPanelTextureID,
			SCREEN_WIDTH * 0.5f,
			WeaponUnlockHud::PanelCenterY + 6.0f,
			WeaponUnlockHud::PanelWidth,
			WeaponUnlockHud::PanelHeight,
			{ 0.02f, 0.01f, 0.04f, alpha * 0.62f });
		DrawDarkRpgPanel(
			m_Impl->SkillPanelTextureID,
			SCREEN_WIDTH * 0.5f,
			WeaponUnlockHud::PanelCenterY,
			WeaponUnlockHud::PanelWidth,
			WeaponUnlockHud::PanelHeight,
			{ 1.0f, 1.0f, 1.0f, alpha });

		const std::size_t weapon_index = static_cast<std::size_t>(
			m_Impl->UnlockedWeaponType);
		const WeaponData& weapon = GameDataManager::GetInstance()
			.GetWeaponGameData().Get(weapon_index);
		Sprite_DrawRegion(
			m_Impl->PlayerHudTextureID,
			WeaponUnlockHud::IconCenterX,
			WeaponUnlockHud::IconCenterY,
			WeaponUnlockHud::IconFrameWidth,
			WeaponUnlockHud::IconFrameHeight,
			0,
			0,
			58,
			60,
			{ 1.0f, 0.90f, 0.62f, alpha });
		const float icon_scale = std::min(
			WeaponUnlockHud::IconMaximumSize / std::max(weapon.Width, 1.0f),
			WeaponUnlockHud::IconMaximumSize / std::max(weapon.Height, 1.0f));
		Sprite_DrawSized(
			m_Impl->WeaponIconTextureIDs[weapon_index],
			WeaponUnlockHud::IconCenterX,
			WeaponUnlockHud::IconCenterY,
			weapon.Width * icon_scale,
			weapon.Height * icon_scale,
			{ 1.0f, 0.90f, 0.58f, alpha });

		if (m_Impl->WeaponUnlockTitleText)
		{
			m_Impl->WeaponUnlockTitleText->Clear();
			m_Impl->WeaponUnlockTitleText->SetText(
				"WEAPON UNLOCKED",
				{ 1.0f, 0.78f, 0.30f, alpha });
			m_Impl->WeaponUnlockTitleText->Draw();
		}
		if (m_Impl->WeaponUnlockNameText)
		{
			m_Impl->WeaponUnlockNameText->Clear();
			m_Impl->WeaponUnlockNameText->SetText(
				GetWeaponDisplayName(m_Impl->UnlockedWeaponType),
				{ 0.94f, 0.96f, 1.0f, alpha });
			m_Impl->WeaponUnlockNameText->Draw();
		}
		m_Impl->WeaponUnlockOkButton.Draw();
	}

	if (has_auto_aim_target && !m_Impl->WeaponUnlockPopupOpen)
	{
		Sprite_DrawSized(
			m_Impl->CrosshairTextureID,
			std::clamp(
				auto_aim_screen_position.x,
				0.0f,
				static_cast<float>(SCREEN_WIDTH)),
			std::clamp(
				auto_aim_screen_position.y,
				0.0f,
				static_cast<float>(SCREEN_HEIGHT)),
			Crosshair::DrawSize,
			Crosshair::DrawSize);
	}
}

void IngameHud::DrawInteractionPrompt(
	const IInteractable& interactable,
	const DirectX::XMFLOAT2& player_position,
	float elapsed_time) const
{
	if (m_Impl->InteractionPromptTextureID == TEXTURE_INVALID_ID ||
		!interactable.CanInteract(player_position))
	{
		return;
	}
	DirectX::XMFLOAT2 prompt_position =
		interactable.GetInteractionPromptPosition();
	prompt_position.y +=
		std::sin(elapsed_time * InteractionPrompt::BobSpeed) *
		InteractionPrompt::BobAmount;
	const bool lighting_was_enabled = Sprite_SetLightingEnabled(false);
	Sprite_DrawSized(
		m_Impl->InteractionPromptTextureID,
		prompt_position.x,
		prompt_position.y,
		InteractionPrompt::DrawSize,
		InteractionPrompt::DrawSize);
	Sprite_SetLightingEnabled(lighting_was_enabled);
}

float IngameHud::GetQSkillCooldownDuration()
{
	return QSkillHud::CooldownDuration;
}
