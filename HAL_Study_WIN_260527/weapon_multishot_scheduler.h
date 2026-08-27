#ifndef WEAPON_MULTISHOT_SCHEDULER_H
#define WEAPON_MULTISHOT_SCHEDULER_H

#include <DirectXMath.h>

enum class BulletType;

using WeaponMultiShotFireCallback = bool (*)(
	BulletType type,
	const DirectX::XMFLOAT2& spawn_position,
	const DirectX::XMFLOAT2& aim_direction);

namespace WeaponMultiShotScheduler
{
	void Reset();
	bool IsActive(BulletType type);
	void Schedule(
		BulletType type,
		int shot_count,
		const DirectX::XMFLOAT2& spawn_offset,
		const DirectX::XMFLOAT2& aim_direction);
	void Update(
		float delta_time,
		const DirectX::XMFLOAT2& owner_position,
		WeaponMultiShotFireCallback fire_callback);
}

#endif // !WEAPON_MULTISHOT_SCHEDULER_H
