#include "game_enemy_internal.h"
#include "game_enemy.h"
#include "Constants/enemy_constants.h"

namespace GameEnemy
{
	using namespace Internal;

	// 적과 적 발사체를 충돌 시스템에 등록한다.
	void RegisterColliders()
	{
		for (int i = 0; i < GameEnemy::ENEMY_CAPACITY; ++i)
		{
			const EnemySlot& slot = g_EnemySystem.EnemySlots[i];
			if (!IsAliveAndTargetable(i))
			{
				continue;
			}
			CollisionSystem_RegisterCircle(i, CollisionLayer::Enemy,
			                               CollisionLayer::Player | CollisionLayer::PlayerBullet,
			                               GetEnemyAimPosition(slot), slot.Entity.GetCollisionRadius());
		}
		for (int projectile_id = 0; projectile_id < EnemyConstants::BossJelly::BulletMax; ++projectile_id)
		{
			const BossJellyBullet& bullet = g_EnemySystem.BossJellyBullets[projectile_id];
			if (!bullet.IsActive || bullet.IsPrimary)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(projectile_id, CollisionLayer::EnemyBullet, CollisionLayer::Player,
			                               bullet.Position, bullet.Radius);
		}
		EnemyAttackPattern::RegisterProjectileColliders(EnemyConstants::BossJelly::BulletMax);
	}

	void HandleCollisionHits(cChainLightning& chain_lightning)
	{
		for (int i = 0; i < CollisionSystem_GetHitCount(); ++i)
		{
			const cCollisionHit* hit = CollisionSystem_GetHit(i);
			if (!hit)
			{
				continue;
			}

			int enemy_id = -1;
			int projectile_id = PROJECTILE_INVALID_ID;
			if (!TryGetEnemyAndProjectile(*hit, enemy_id, projectile_id) || !IsValidEnemyID(enemy_id) ||
			    !g_EnemySystem.EnemySlots[enemy_id].Entity.IsAlive() || !ProjectileSystem_IsActive(projectile_id))
			{
				continue;
			}

			if (!ProjectileSystem_TryRegisterTargetHit(projectile_id, enemy_id))
			{
				continue;
			}

			const cProjectile* projectile = ProjectileSystem_GetProjectile(projectile_id);
			if (!projectile)
			{
				continue;
			}

			const DirectX::XMFLOAT2 enemy_position = GetEnemyAimPosition(g_EnemySystem.EnemySlots[enemy_id]);
			switch (projectile->HitBehavior)
			{
			case ProjectileHitBehavior::Area:
				ApplyAreaProjectileDamage(projectile->Position, projectile->AreaRadius, projectile->Damage,
				                          projectile->Velocity);
				break;

			case ProjectileHitBehavior::ChainLightning:
				ApplyProjectileDamage(enemy_id, projectile->Damage, projectile->Velocity,
				                      EnemyConstants::Knockback::PlayerBulletKnockbackSpeed);
				chain_lightning.TryTrigger(enemy_id, enemy_position, projectile->Damage);
				break;

			case ProjectileHitBehavior::Pierce:
			case ProjectileHitBehavior::PersistentPierce:
				ApplyProjectileDamage(enemy_id, projectile->Damage, projectile->Velocity,
				                      EnemyConstants::Knockback::PlayerBulletKnockbackSpeed);
				cGameEffectManager::GetInstance().Play(GameEffectType::VoidImplosion, enemy_position, 0.72f);
				break;

			case ProjectileHitBehavior::Stop:
			default:
				ApplyProjectileDamage(enemy_id, projectile->Damage, projectile->Velocity,
				                      EnemyConstants::Knockback::PlayerBulletKnockbackSpeed);
				break;
			}

			if (projectile->HitBehavior == ProjectileHitBehavior::PersistentPierce)
			{
				continue;
			}
			if (projectile->HitBehavior != ProjectileHitBehavior::Pierce ||
			    projectile->TargetHitCount >= projectile->MaxTargetHits)
			{
				ProjectileSystem_Deactivate(projectile_id);
			}
		}
	}

	void Deactivate(int enemy_id)
	{
		if (!IsValidEnemyID(enemy_id))
		{
			return;
		}
		g_EnemySystem.EnemySlots[enemy_id].Entity.Deactivate();
		g_EnemySystem.EnemySlots[enemy_id].RoomIndex = -1;
		EnemyAttackPattern::OnDeactivate(enemy_id);
	}

	bool ConsumeEnemyBullet(int projectile_id, float& out_damage)
	{
		if (projectile_id >= EnemyConstants::BossJelly::BulletMax)
		{
			return EnemyAttackPattern::ConsumeProjectile(projectile_id - EnemyConstants::BossJelly::BulletMax,
			                                             out_damage);
		}
		if (projectile_id < 0)
		{
			return false;
		}
		BossJellyBullet& bullet = g_EnemySystem.BossJellyBullets[projectile_id];
		if (!bullet.IsActive)
		{
			return false;
		}
		out_damage = bullet.Damage;
		bullet.IsActive = false;
		return true;
	}

	bool IsActive(int enemy_id)
	{
		return IsValidEnemyID(enemy_id) && g_EnemySystem.EnemySlots[enemy_id].Entity.IsActive();
	}
} // namespace GameEnemy
