#include "game_bullet.h"

#include <algorithm>

#include "game_player.h"
#include "projectile.h"
#include "weapon_audio.h"
#include "weapon_fire_controller.h"
#include "weapon_inventory.h"
#include "weapon_projectile_factory.h"

namespace GameBullet
{
	void Initialize()
	{
		ProjectileSystem_Initialize();
		WeaponProjectileFactory::Initialize();
		WeaponAudio::Initialize();
		WeaponFireController::Initialize();
	}

	void Finalize()
	{
		WeaponFireController::Finalize();
		ProjectileSystem_Finalize();
		WeaponAudio::Finalize();
		WeaponProjectileFactory::Finalize();
	}

	bool UnlockRandomWeapon()
	{
		return WeaponFireController::UnlockRandomWeapon();
	}

	bool ConsumeUnlockedWeapon(BulletType& out_type)
	{
		return WeaponInventory::ConsumeUnlockedWeapon(out_type);
	}

	bool IsWeaponOwned(BulletType type)
	{
		return WeaponInventory::IsOwned(type);
	}

	int GetWeaponTextureID(BulletType type)
	{
		return WeaponProjectileFactory::GetTextureID(type);
	}

	void IncreaseProjectileCount(BulletType type)
	{
		WeaponFireController::IncreaseProjectileCount(type);
	}

	void MultiplyAttackSpeed(BulletType type, float multiplier)
	{
		WeaponFireController::MultiplyAttackSpeed(type, multiplier);
	}

	void MultiplyDamage(BulletType type, float multiplier)
	{
		WeaponFireController::MultiplyDamage(type, multiplier);
	}

	void PlayFireballExplosionSound()
	{
		WeaponAudio::PlayFireballExplosion();
	}

	bool Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& target_position)
	{
		return WeaponFireController::Fire(spawn_position, target_position);
	}

	void Update(float delta_time)
	{
		delta_time = std::max(delta_time, 0.0f);
		WeaponFireController::UpdateCooldowns(delta_time);
		WeaponFireController::UpdateMultiShotBursts(delta_time, GamePlayer::GetPosition());
		WeaponFireController::MaintainPassiveWeapons(GamePlayer::GetPosition());
		ProjectileSystem_Update(delta_time, GamePlayer::GetPosition());
		WeaponAudio::UpdateProjectileEvents();
	}

	void Clear()
	{
		ProjectileSystem_Clear();
		WeaponFireController::Clear();
	}

	void Draw()
	{
		ProjectileSystem_Draw();
	}

	void RegisterColliders()
	{
		ProjectileSystem_RegisterColliders();
	}
} // namespace GameBullet
