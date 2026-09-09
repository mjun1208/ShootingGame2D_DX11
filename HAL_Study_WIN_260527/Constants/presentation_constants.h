#ifndef PRESENTATION_CONSTANTS_H
#define PRESENTATION_CONSTANTS_H

#include "config.h"

namespace PresentationConstants
{
	namespace Intro
	{
		inline constexpr int SourceY = 229;
		inline constexpr int SourceWidth = 2161;
		inline constexpr int SourceHeight = 251;
		inline constexpr float TextureHeight = 724.0f;
		inline constexpr float BandHeight = 148.0f;
		inline constexpr float UpperTargetY = SCREEN_HEIGHT * 0.29f;
		inline constexpr float LowerTargetY = SCREEN_HEIGHT * 0.71f;
		inline constexpr float SlideInDuration = 0.25f;
		inline constexpr float SlideOutStart = 1.72f;
		inline constexpr float SlideOutDuration = 0.28f;
		inline constexpr float TotalDuration = SlideOutStart + SlideOutDuration;
		inline constexpr float ScrollSpeed = 245.0f;
		inline constexpr float BlinkCyclesPerSecond = 3.0f;
		inline constexpr float Pi = 3.14159265358979323846f;

	} // namespace Intro

	namespace PlayerDeath
	{
		inline constexpr float ShakeSize = 22.0f;
		inline constexpr float ShakeDuration = 0.65f;
		inline constexpr float Zoom = 2.4f;
		inline constexpr float ZoomDuration = 1.35f;
		inline constexpr float IrisDelay = 0.20f;
		inline constexpr float IrisDuration = 1.35f;
		inline constexpr float IrisBandHeight = 8.0f;
		inline constexpr float AfterAnimationHold = 0.25f;

	} // namespace PlayerDeath

	namespace BossDeath
	{
		inline constexpr float Zoom = 1.55f;
		inline constexpr float ZoomInDuration = 0.42f;
		inline constexpr float ExplosionStartTime = 0.48f;
		inline constexpr int SmallExplosionCount = 20;
		inline constexpr float SmallExplosionSequenceDuration = 1.30f;
		inline constexpr float FinalExplosionDelay = 0.32f;
		inline constexpr float FinalHoldDuration = 0.40f;
		inline constexpr float ZoomOutDuration = 0.55f;
		inline constexpr float SmallExplosionShakeSize = 4.5f;
		inline constexpr float SmallExplosionShakeDuration = 0.08f;
		inline constexpr float FinalExplosionShakeSize = 30.0f;
		inline constexpr float FinalExplosionShakeDuration = 0.62f;
		inline constexpr float ZoomOutShakeSize = 12.0f;
		inline constexpr float ImplosionLeadTime = 0.24f;
		inline constexpr float FinalExplosionTime =
		    ExplosionStartTime + SmallExplosionSequenceDuration + FinalExplosionDelay;
		inline constexpr float ZoomOutStartTime = FinalExplosionTime + FinalHoldDuration;
		inline constexpr float EndTime = ZoomOutStartTime + ZoomOutDuration;

	} // namespace BossDeath
} // namespace PresentationConstants

namespace PresentationConstants::TextAndEffects
{
	inline constexpr float glyph_width = 40.0f;
	inline constexpr float glyph_height = 64.0f;
	inline constexpr float character_spacing = 52.0f;
	inline constexpr int FinalRingExplosionCount = 12;
} // namespace PresentationConstants::TextAndEffects

#endif // PRESENTATION_CONSTANTS_H
