#ifndef ENEMY_PATTERN_CONSTANTS_H
#define ENEMY_PATTERN_CONSTANTS_H

#include "game_enemy.h"
#include <DirectXMath.h>

namespace EnemyPatternConstants::BoneProjectile
{
	inline constexpr int Capacity = 256;
	inline constexpr float Speed = 430.0f;
	inline constexpr float SpinSpeed = 8.5f;
} // namespace EnemyPatternConstants::BoneProjectile

namespace EnemyPatternConstants::DaggerProjectile
{
	inline constexpr int Capacity = 128;
	inline constexpr float Speed = 560.0f;
	inline constexpr float MaxDistance = 820.0f;
	inline constexpr float Damage = 6.0f;
	inline constexpr float Radius = 9.0f;
	inline constexpr float Size = 32.0f;
} // namespace EnemyPatternConstants::DaggerProjectile

namespace EnemyPatternConstants::MageProjectile
{
	inline constexpr int Capacity = 128;
	inline constexpr float Speed = 520.0f;
	inline constexpr float MaxDistance = 920.0f;
	inline constexpr float Radius = 12.0f;
} // namespace EnemyPatternConstants::MageProjectile

namespace EnemyPatternConstants::Warrior
{
	inline constexpr int StrikeCapacity = 128;
	inline constexpr float AttackWindupDuration = 0.30f;
	inline constexpr float StrikeRadius = 54.0f;
	inline constexpr float StrikeLifetime = 0.12f;
} // namespace EnemyPatternConstants::Warrior

namespace EnemyPatternConstants::GroundHazard
{
	inline constexpr int Capacity = 64;
} // namespace EnemyPatternConstants::GroundHazard

namespace EnemyPatternConstants::Slime
{
	inline constexpr float DashTelegraphDuration = 0.58f;
} // namespace EnemyPatternConstants::Slime

namespace EnemyPatternConstants::Bat
{
	inline constexpr float DashWindupDuration = 0.45f;
} // namespace EnemyPatternConstants::Bat

namespace EnemyPatternConstants::SkeletonMage
{
	inline constexpr float CastDuration = 0.92f;
} // namespace EnemyPatternConstants::SkeletonMage

namespace EnemyPatternConstants::SkeletonWarrior
{
	inline constexpr float StrikeDamage = 10.0f;
} // namespace EnemyPatternConstants::SkeletonWarrior

namespace EnemyPatternConstants::OrcWarrior
{
	inline constexpr float StrikeDamage = 14.0f;
	inline constexpr float SidestepDuration = 0.10f;
	inline constexpr float SidestepForwardWeight = 0.78f;
	inline constexpr float SidestepSideWeight = 0.625f;
	inline constexpr int DashTrailCount = 2;
	inline constexpr float DashTrailDuration = 0.22f;
} // namespace EnemyPatternConstants::OrcWarrior

namespace EnemyPatternConstants::RangedMovement
{
	inline constexpr float RetreatDistance = 300.0f;
	inline constexpr float ApproachDistance = 520.0f;
} // namespace EnemyPatternConstants::RangedMovement

namespace EnemyPatternConstants::Rogue
{
	inline constexpr float WindupDuration = 0.72f;
	inline constexpr float StrikeRadius = 52.0f;
	inline constexpr float StrikeDamage = 6.0f;
} // namespace EnemyPatternConstants::Rogue

namespace EnemyPatternConstants::SkeletonRogue
{
	inline constexpr float WindupDuration = 0.38f;
	inline constexpr float DaggerAngleStep = 0.14f;
} // namespace EnemyPatternConstants::SkeletonRogue

namespace EnemyPatternConstants::OrcAxe
{
	inline constexpr float ThrowWindupDuration = 0.48f;
	inline constexpr float ProjectileSpeed = 500.0f;
	inline constexpr float ProjectileMaxDistance = 920.0f;
	inline constexpr float ProjectileDamage = 9.0f;
	inline constexpr float ProjectileRadius = 17.0f;
	inline constexpr float ProjectileSize = 58.0f;
	inline constexpr float ProjectileSpinSpeed = 10.5f;
} // namespace EnemyPatternConstants::OrcAxe

namespace EnemyPatternConstants::Shaman
{
	inline constexpr float CastDuration = 0.82f;
	inline constexpr float HazardRadius = 92.0f;
	inline constexpr float HazardDamage = 12.0f;
	inline constexpr float HazardLifetime = 0.30f;
} // namespace EnemyPatternConstants::Shaman

namespace EnemyPatternConstants::Cthulhu
{
	inline constexpr float RoamMinDuration = 0.95f;
	inline constexpr float RoamMaxDuration = 1.40f;
	inline constexpr int PatternCount = 5;
	inline constexpr float VanishDuration = 0.36f;
	inline constexpr float DashWindupDuration = 0.76f;
} // namespace EnemyPatternConstants::Cthulhu

#endif // ENEMY_PATTERN_CONSTANTS_H
