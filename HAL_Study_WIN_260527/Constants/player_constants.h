#ifndef PLAYER_CONSTANTS_H
#define PLAYER_CONSTANTS_H

namespace PlayerConstants
{
	namespace Stats
	{
		inline constexpr float MaxHitPoint = 100.0f;
		inline constexpr float MoveSpeed = 500.0f;
		inline constexpr float ContactDamage = 10.0f;
		inline constexpr float DamageInvincibleDuration = 0.65f;
		inline constexpr float DamageFlashDuration = 0.12f;

	} // namespace Stats

	namespace Animation
	{
		struct Description
		{
			int FrameCount;
			int FrameWidth;
			int FrameHeight;
			float FrameDuration;
		};

		inline constexpr float SpriteScale = 3.0f;
		inline constexpr Description Idle{ 4, 32, 32, 0.14f };
		inline constexpr Description Run{ 6, 64, 64, 0.08f };
		inline constexpr Description Die{ 6, 64, 32, 0.12f };
		inline constexpr float RunDrawYOffset = -16.0f * SpriteScale;

	} // namespace Animation

	namespace Movement
	{
		inline constexpr float AimDeadZoneSq = 16.0f;
		inline constexpr float CollisionRadius = 30.0f;
		inline constexpr float FeetYOffset = 48.0f;
		inline constexpr float SprintSpeedMultiplier = 1.65f;
		inline constexpr float SprintDustInterval = 0.10f;

	} // namespace Movement

	namespace Dash
	{
		struct Description
		{
			float Distance;
			float Duration;
			float Cooldown;
			float EffectDuration;
		};

		inline constexpr Description Normal{ 240.0f, 0.18f, 0.8f, 0.22f };
		inline constexpr Description Empowered{ 560.0f, 0.12f, 0.12f, 0.48f };
		inline constexpr int AfterimageCount = 6;
		inline constexpr int EmpoweredCharges = 3;

	} // namespace Dash

	namespace Slash
	{
		inline constexpr float TrailLifetime = 0.72f;
		inline constexpr float LaserDrawDuration = 0.065f;
		inline constexpr float LaserFadeDuration = 0.34f;
		inline constexpr float CutInterval = 0.055f;
		inline constexpr int CutCount = 5;
		inline constexpr int TrailMax = 6;
		inline constexpr int FrameSize = 128;
		inline constexpr int ArcFrameCount = 9;
		inline constexpr int ChainFrameCount = 7;
		inline constexpr float FrameTime = 0.028f;
		inline constexpr float CutFrameTime = 0.022f;
		inline constexpr float CutVisibleDuration = 0.18f;
		inline constexpr int StreaksPerHit = 3;

	} // namespace Slash
} // namespace PlayerConstants

namespace PlayerConstants::Slash
{
	inline constexpr float SlashHalfWidth = 46.0f;
	inline constexpr float DamagePerHit = 8.0f;
} // namespace PlayerConstants::Slash

#endif // PLAYER_CONSTANTS_H
