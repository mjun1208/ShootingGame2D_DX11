#ifndef WEAPON_DATA_H
#define WEAPON_DATA_H

#include "projectile.h"

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <string>

struct WeaponData
{
	std::string Id;
	std::wstring TexturePath;
	float Width;
	float Height;
	float CollisionRadius;
	float ProjectileSpeed;
	float Damage;
	float FireInterval;
	float ProjectileLifeTime;
	ProjectileHitBehavior HitBehavior;
	float AreaRadius;
	int MaxTargetHits;
	int MaxBounces;
	int AliveLimitGroup;
	int MaxAlive;
	int VolleyCount;
	float VolleySpawnRadius;
	float VolleyArcDegrees{ 360.0f };
	bool UsesBezierHoming;
	float BezierCurveStrength;
	ProjectileMotionBehavior MotionBehavior{ ProjectileMotionBehavior::Linear };
	float BoomerangReturnTime{ 0.0f };
	float OrbitRadius{ 0.0f };
	float OrbitAngularSpeed{ 0.0f };
	float SpinSpeed{ 0.0f };
	float RepeatHitInterval{ 0.0f };
	float MagicBladeSummonTime{ 0.0f };
	float MagicBladeReadyDelay{ 0.2f };
	float MagicBladeSideOffset{ 0.0f };
	bool UsesTrail{ true };
	float TrailEmitInterval;
	float TrailWidth;
	float TrailLifeTime;
	float TrailStartScale;
	float TrailEndScale;
	DirectX::XMFLOAT4 TrailColor;
};

class WeaponGameData
{
  public:
	static constexpr std::size_t WEAPON_COUNT = 8;

	bool Load(const char* file_path);
	const WeaponData& Get(std::size_t weapon_index) const;
	// Missing or unloaded data returns nullptr; Get throws std::out_of_range.
	const WeaponData* Find(std::size_t weapon_index) const;

  private:
	std::array<WeaponData, WEAPON_COUNT> m_Weapons{};
	bool m_IsLoaded{ false };
};

#endif // WEAPON_DATA_H
