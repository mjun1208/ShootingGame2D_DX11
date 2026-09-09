#ifndef ENEMY_CONSTANTS_H
#define ENEMY_CONSTANTS_H

#include "game_enemy.h"
#include "Constants/player_constants.h"
#include <DirectXMath.h>
#include <array>

namespace EnemyConstants::SlimeRush
{
	inline constexpr int Round = 4;
} // namespace EnemyConstants::SlimeRush

namespace EnemyConstants::Knockback
{
	inline constexpr float PlayerBulletKnockbackSpeed = 100.0f;
} // namespace EnemyConstants::Knockback

namespace EnemyConstants::Audio
{
	inline constexpr std::array<const char*, 3> DamageSoundPaths{
		"asset/sound/leohpaz-21-orc-damage-1.wav",
		"asset/sound/leohpaz-21-orc-damage-2.wav",
		"asset/sound/leohpaz-21-orc-damage-3.wav",
	};
} // namespace EnemyConstants::Audio

namespace EnemyConstants::Spawn
{
	inline constexpr float TelegraphDuration = 0.5f;
	inline constexpr float TelegraphSize = 92.0f;
	inline constexpr float ArrivalRingDuration = 0.24f;
	inline constexpr MonsterType DefaultMonsterType = MonsterType::SkeletonBase;
} // namespace EnemyConstants::Spawn

namespace EnemyConstants::Collision
{
	inline constexpr float WallPadding = 8.0f;
	inline constexpr int GridBucketCount = 257;
} // namespace EnemyConstants::Collision

namespace EnemyConstants::BossJelly
{
	inline constexpr int BulletMax = 768;
	inline constexpr float FireInterval = 2.55f;
	inline constexpr float PrimaryDistance = 420.0f;
	inline constexpr float TelegraphDuration = 0.65f;
	inline constexpr float FireScaleDuration = 0.28f;
} // namespace EnemyConstants::BossJelly

namespace EnemyConstants::BossBody
{
	inline constexpr float SplitScaleDuration = 0.36f;
} // namespace EnemyConstants::BossBody

#endif // ENEMY_CONSTANTS_H
