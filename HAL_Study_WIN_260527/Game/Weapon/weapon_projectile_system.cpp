#include "game_data_manager.h"
#include "Constants/weapon_constants.h"
#include "game_effect.h"
#include "texture.h"
#include "weapon_audio.h"
#include "weapon_data.h"
#include "weapon_fire_context.h"
#include "weapon_magic_blade.h"
#include "weapon_projectile_factory.h"
#include <array>
#include <cmath>

namespace
{
	constexpr int PLAYER_OWNER_ID = 0;
	constexpr int WEAPON_COUNT = static_cast<int>(BulletType::Count);
	static_assert(WEAPON_COUNT == static_cast<int>(WeaponGameData::WEAPON_COUNT));

	std::array<int, WEAPON_COUNT> g_TextureIDs{};
	int g_TrailTextureID = TEXTURE_INVALID_ID;
} // namespace

namespace WeaponProjectileFactory
{
	void Initialize()
	{
		g_TextureIDs.fill(TEXTURE_INVALID_ID);
		for (int i = 0; i < WEAPON_COUNT; ++i)
		{
			const WeaponData& weapon_data = GetWeaponData(static_cast<std::size_t>(i));
			g_TextureIDs[i] = Texture_Load(weapon_data.TexturePath.c_str());
		}
		// 투사체 잔상은 가장자리가 선명한 블록으로 그리며,
		// 잔상 시스템에서 크기와 알파를 단계적으로 바꾼다.
		g_TrailTextureID = Texture_Load(L"asset/texture/white_square.png");
	}

	void Finalize()
	{
		Texture_Release(g_TrailTextureID);
		g_TrailTextureID = TEXTURE_INVALID_ID;
		for (int& texture_id : g_TextureIDs)
		{
			Texture_Release(texture_id);
			texture_id = TEXTURE_INVALID_ID;
		}
	}

	int GetTextureID(BulletType type)
	{
		const int type_index = static_cast<int>(type);
		return type_index >= 0 && type_index < WEAPON_COUNT ? g_TextureIDs[type_index] : TEXTURE_INVALID_ID;
	}

	cProjectileDesc Build(BulletType type, const WeaponData& profile, const DirectX::XMFLOAT2& spawn_position,
	                      const DirectX::XMFLOAT2& normalized_direction, float damage_multiplier,
	                      float speed_multiplier, float lifetime_multiplier)
	{
		cProjectileDesc desc{};
		desc.Position = spawn_position;
		desc.Velocity = {
			normalized_direction.x * profile.ProjectileSpeed * speed_multiplier,
			normalized_direction.y * profile.ProjectileSpeed * speed_multiplier,
		};
		desc.Radius = profile.CollisionRadius;
		desc.Width = profile.Width;
		desc.Height = profile.Height;
		desc.Rotation = std::atan2(normalized_direction.x, -normalized_direction.y);
		desc.Damage = profile.Damage * damage_multiplier;
		desc.LifeTime = profile.ProjectileLifeTime * lifetime_multiplier;
		const int type_index = static_cast<int>(type);
		desc.TextureID = type_index >= 0 && type_index < WEAPON_COUNT ? g_TextureIDs[type_index] : TEXTURE_INVALID_ID;
		desc.OwnerID = PLAYER_OWNER_ID;
		desc.Layer = CollisionLayer::PlayerBullet;
		desc.HitMask = CollisionLayer::Enemy;
		desc.HitBehavior = profile.HitBehavior;
		desc.AreaRadius = profile.AreaRadius;
		desc.MaxTargetHits = profile.MaxTargetHits;
		desc.MaxBounces = profile.MaxBounces;
		if (profile.MaxAlive > 0)
		{
			desc.AliveLimitGroup = profile.AliveLimitGroup;
			desc.MaxAliveInGroup = profile.MaxAlive;
		}
		desc.UsesBezierHoming = profile.UsesBezierHoming;
		desc.BezierCurveStrength = profile.BezierCurveStrength;
		desc.MotionBehavior = profile.MotionBehavior;
		desc.BoomerangReturnTime = profile.BoomerangReturnTime;
		desc.OrbitRadius = profile.OrbitRadius;
		desc.OrbitAngularSpeed = profile.OrbitAngularSpeed;
		desc.OrbitPhase = std::atan2(normalized_direction.y, normalized_direction.x);
		desc.SpinSpeed = profile.SpinSpeed;
		desc.RepeatHitInterval = profile.RepeatHitInterval;
		desc.MagicBladeSummonTime = profile.MagicBladeSummonTime;
		desc.MagicBladeReadyDelay = profile.MagicBladeReadyDelay;
		desc.MagicBladeSideOffset = profile.MagicBladeSideOffset;
		desc.UsesTrail = profile.UsesTrail;
		desc.TrailTextureID = g_TrailTextureID;
		desc.TrailEmitInterval = profile.TrailEmitInterval;
		desc.TrailWidth = profile.TrailWidth;
		desc.TrailLifeTime = profile.TrailLifeTime;
		desc.TrailStartScale = profile.TrailStartScale;
		desc.TrailEndScale = profile.TrailEndScale;
		desc.TrailColor = profile.TrailColor;
		return desc;
	}

	bool Submit(const cProjectileDesc& desc)
	{
		return ProjectileSystem_Fire(desc) != PROJECTILE_INVALID_ID;
	}
} // namespace WeaponProjectileFactory

namespace
{
	float g_MagicBladeSpawnSide = 1.0f;
} // namespace

namespace WeaponMagicBlade
{
	void Reset()
	{
		g_MagicBladeSpawnSide = 1.0f;
	}

	bool Fire(const WeaponFireContext& context)
	{
		const DirectX::XMFLOAT2& spawn_position = context.SpawnPosition;
		const DirectX::XMFLOAT2& aim_direction = context.AimDirection;
		const int projectile_count = context.Profile.VolleyCount > 0 ? context.Profile.VolleyCount : 1;
		bool fired_any = false;
		for (int i = 0; i < projectile_count; ++i)
		{
			cProjectileDesc desc = context.BuildProjectile(spawn_position, aim_direction);
			desc.BezierCurveDirection = g_MagicBladeSpawnSide;
			desc.MagicBladeSideOffset *= 1.0f + static_cast<float>(i / 2) * 0.35f;
			desc.Position = {
				spawn_position.x - aim_direction.x * WeaponConstants::Projectile::SummonBackOffset,
				spawn_position.y - aim_direction.y * WeaponConstants::Projectile::SummonBackOffset,
			};
			g_MagicBladeSpawnSide = -g_MagicBladeSpawnSide;
			if (!context.SubmitProjectile(desc))
			{
				continue;
			}

			fired_any = true;
			cGameEffectManager::GetInstance().Play(GameEffectType::PixelMagicHit, desc.Position, 0.38f,
			                                       { 0.08f, 0.62f, 1.0f, 0.88f }, desc.Rotation);
		}

		if (fired_any)
		{
			WeaponAudio::PlayMagicBladeSummon();
		}
		return fired_any;
	}
} // namespace WeaponMagicBlade
