#ifndef MENU_CONSTANTS_H
#define MENU_CONSTANTS_H

namespace MenuConstants
{
	namespace Panel
	{
		inline constexpr int TextureSize = 24;
		inline constexpr int SourceBorder = 4;
		inline constexpr float DrawBorder = 16.0f;

	} // namespace Panel

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

		inline constexpr float PanelWidth = 540.0f;
		inline constexpr float PanelHeight = 720.0f;
		inline constexpr float ButtonWidth = 360.0f;
		inline constexpr float ButtonHeight = 82.0f;
		inline constexpr float FirstButtonY = 360.0f;
		inline constexpr float ButtonGap = 110.0f;

	} // namespace Pause
} // namespace MenuConstants

namespace MenuConstants::Augment
{
	inline constexpr int ChoiceCount = 3;
	inline constexpr float PanelWidth = 1320.0f;
	inline constexpr float PanelHeight = 820.0f;
	inline constexpr float CardWidth = 340.0f;
	inline constexpr float CardHeight = 570.0f;
	inline constexpr float CardGap = 48.0f;
	inline constexpr float CardCenterY = 535.0f;
	inline constexpr float IconCenterY = 365.0f;
	inline constexpr float IconFrameSize = 156.0f;
	inline constexpr float IconMaximumSize = 116.0f;
	inline constexpr float WeaponNameY = 455.0f;
	inline constexpr float EffectDetailY = 535.0f;
	inline constexpr float SelectButtonY = 720.0f;
	inline constexpr float SelectButtonWidth = 230.0f;
	inline constexpr float SelectButtonHeight = 66.0f;
} // namespace MenuConstants::Augment

#endif // MENU_CONSTANTS_H
