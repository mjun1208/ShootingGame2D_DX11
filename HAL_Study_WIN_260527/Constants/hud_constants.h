#ifndef HUD_CONSTANTS_H
#define HUD_CONSTANTS_H

#include "config.h"
#include "game_bullet.h"
#include "sprite_region.h"
#include <cstddef>

namespace HudConstants
{
	namespace Crosshair
	{
		inline constexpr float DrawSize = 72.0f;

	} // namespace Crosshair

	namespace InteractionPrompt
	{
		inline constexpr float DrawSize = 52.0f;
		inline constexpr float BobAmount = 5.0f;
		inline constexpr float BobSpeed = 6.0f;

	} // namespace InteractionPrompt

	namespace Player
	{
		inline constexpr float Scale = 3.0f;
		inline constexpr float Margin = 28.0f;
		inline constexpr SpriteRegion FrameSource{ 58, 60, 145, 16 };
		inline constexpr int FrameLeftCapWidth = 12;
		inline constexpr int FrameRightCapWidth = 11;
		inline constexpr int FrameRightCapX = FrameSource.X + FrameSource.Width - FrameRightCapWidth;
		inline constexpr int FrameCenterSourceX = FrameSource.X + FrameLeftCapWidth;
		inline constexpr float BarGap = 8.0f;
		inline constexpr SpriteRegion HealthFillSource{ 70, 44, 112, 15 };
		inline constexpr SpriteRegion ExperienceFillSource{ 58, 77, 59, 12 };
		inline constexpr float LevelTextGap = 18.0f;
		inline constexpr float ValueCharacterSpacing = 18.0f;
		inline constexpr int ValueCharacterCount = 9;

	} // namespace Player

	namespace Boss
	{
		inline constexpr float Width = 1040.0f;
		inline constexpr float FrameHeight = 58.0f;
		inline constexpr float BottomMargin = 34.0f;
		inline constexpr float NameGap = 18.0f;
		inline constexpr float NameCharacterSpacing = 30.0f;
		inline constexpr float ValueCharacterSpacing = 20.0f;
		inline constexpr int ValueCharacterCount = 9;

	} // namespace Boss

	namespace Round
	{
		inline constexpr float GlyphWidth = 20.0f;
		inline constexpr float GlyphHeight = 32.0f;
		inline constexpr float CharacterSpacing = 18.0f;
		inline constexpr float TopInset = 10.0f;
		inline constexpr float ShadowOffset = 2.0f;

	} // namespace Round

	namespace Skill
	{
		inline constexpr float CooldownDuration = 8.0f;
		inline constexpr float SlotSize = 88.0f;
		inline constexpr float InnerSize = 60.0f;
		inline constexpr float IconSize = 62.0f;
		inline constexpr float HudGap = 16.0f;
		inline constexpr float CenterX = Player::Margin + SlotSize * 0.5f;
		inline constexpr float CenterY = Player::Margin + Player::FrameSource.Height * Player::Scale * 2.0f +
		                                 Player::BarGap + HudGap + SlotSize * 0.5f;
		inline constexpr int PanelTextureSize = 24;
		inline constexpr int PanelCenterSource = PanelTextureSize / 2;

	} // namespace Skill

	namespace WeaponUnlock
	{
		inline constexpr float PanelWidth = 720.0f;
		inline constexpr float PanelHeight = 470.0f;
		inline constexpr float PanelCenterY = SCREEN_HEIGHT * 0.5f;
		inline constexpr float IconCenterX = SCREEN_WIDTH * 0.5f;
		inline constexpr float IconCenterY = PanelCenterY - 42.0f;
		inline constexpr float IconFrameWidth = 144.0f;
		inline constexpr float IconFrameHeight = 148.0f;
		inline constexpr float IconMaximumSize = 100.0f;
		inline constexpr float OkButtonWidth = 240.0f;
		inline constexpr float OkButtonHeight = 68.0f;
		inline constexpr float OkButtonCenterY = PanelCenterY + 164.0f;
		inline constexpr std::size_t WeaponCount = static_cast<std::size_t>(BulletType::Count);

	} // namespace WeaponUnlock

	namespace Panel
	{
		inline constexpr int TextureSize = 24;
		inline constexpr int SourceBorder = 4;
		inline constexpr float DrawBorder = 18.0f;

	} // namespace Panel
} // namespace HudConstants

#endif // HUD_CONSTANTS_H
