#ifndef ITEM_CONSTANTS_H
#define ITEM_CONSTANTS_H

namespace ItemConstants::Chest
{
	inline constexpr int ChestFrameWidth = 48;
	inline constexpr int ChestFrameHeight = 48;
	inline constexpr int ChestFirstOpenFrame = 0;
	inline constexpr int ChestTextureY = 0;
	inline constexpr int ChestFrameCount = 8;
	inline constexpr float ChestFrameTime = 0.075f;
	inline constexpr float ChestDrawWidth = 144.0f;
	inline constexpr float ChestDrawHeight = 144.0f;
	inline constexpr float ChestInteractionRadius = 150.0f;
	inline constexpr float ChestPromptMargin = -10.0f;
	inline constexpr float ChestFallbackHealAmount = 30.0f;
	inline constexpr float ChestFallbackExperienceRatio = 0.20f;
} // namespace ItemConstants::Chest

namespace ItemConstants::Healing
{
	inline constexpr int HealingItemMax = 128;
	inline constexpr float HealingItemDropChance = 0.12f;
	inline constexpr float HealingItemHealAmount = 25.0f;
	inline constexpr float HealingItemAttractionRadius = 230.0f;
	inline constexpr float HealingItemAttractionRadiusSq = HealingItemAttractionRadius * HealingItemAttractionRadius;
	inline constexpr float HealingItemAbsorbRadius = 38.0f;
	inline constexpr float HealingItemAbsorbRadiusSq = HealingItemAbsorbRadius * HealingItemAbsorbRadius;
	inline constexpr float HealingItemStartSpeed = 92.0f;
	inline constexpr float HealingItemScatterDeceleration = 320.0f;
	inline constexpr float HealingItemMagnetStartSpeed = 240.0f;
	inline constexpr float HealingItemMagnetAcceleration = 1350.0f;
	inline constexpr float HealingItemMagnetMaxSpeed = 960.0f;
	inline constexpr float HealingItemDrawSize = 38.0f;
	inline constexpr float HealingItemGlowSize = 54.0f;
	inline constexpr float HealingItemBobAmount = 3.0f;
} // namespace ItemConstants::Healing

namespace ItemConstants::Experience
{
	inline constexpr int ExperienceGemMax = 4096;
	inline constexpr int ExperienceGemGridBucketCount = 521;
	inline constexpr int ExperienceGemInvalidIndex = -1;
	inline constexpr float ExperienceGemGridCellSize = 192.0f;
	inline constexpr float ExperienceGemAttractionRadius = 280.0f;
	inline constexpr float ExperienceGemAttractionRadiusSq =
	    ExperienceGemAttractionRadius * ExperienceGemAttractionRadius;
	inline constexpr float ExperienceGemAbsorbRadius = 36.0f;
	inline constexpr float ExperienceGemAbsorbRadiusSq = ExperienceGemAbsorbRadius * ExperienceGemAbsorbRadius;
	inline constexpr float ExperienceGemStartSpeed = 360.0f;
	inline constexpr float ExperienceGemScatterDeceleration = 420.0f;
	inline constexpr float ExperienceGemGoldenAngle = 2.39996323f;
	inline constexpr float ExperienceGemMagnetStartSpeed = 260.0f;
	inline constexpr float ExperienceGemMagnetAcceleration = 1500.0f;
	inline constexpr float ExperienceGemMagnetMaxSpeed = 1050.0f;
	inline constexpr float ExperienceGemDrawSize = 24.0f;
	inline constexpr float ExperienceGemGlowSize = 40.0f;
	inline constexpr float ExperienceGemBobAmount = 2.5f;
} // namespace ItemConstants::Experience

namespace ItemConstants::Portal
{
	inline constexpr int PortalFrameWidth = 32;
	inline constexpr int PortalFrameHeight = 32;
	inline constexpr int PortalOpenColumns = 4;
	inline constexpr int PortalOpenFrameCount = 17;
	inline constexpr int PortalIdleFrameCount = 5;
	inline constexpr float PortalOpenFrameTime = 0.055f;
	inline constexpr float PortalIdleFrameTime = 0.10f;
	inline constexpr float PortalDrawSize = 160.0f;
	inline constexpr float PortalInteractionRadius = 120.0f;
	inline constexpr float PortalPromptOffsetY = 112.0f;
} // namespace ItemConstants::Portal

#endif // ITEM_CONSTANTS_H
