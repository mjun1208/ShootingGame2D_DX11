#include "weapon_magic_blade.h"

#include "game_effect.h"
#include "weapon_audio.h"
#include "weapon_data.h"
#include "weapon_fire_strategy.h"

namespace
{
	constexpr float SUMMON_BACK_OFFSET = 42.0f;

	class MagicBladeFireStrategy final : public IWeaponFireStrategy
	{
	public:
		void Reset() override
		{
			m_SpawnSide = 1.0f;
		}

		bool Fire(const WeaponFireContext& context) override
		{
			const DirectX::XMFLOAT2& spawn_position =
				context.GetSpawnPosition();
			const DirectX::XMFLOAT2& aim_direction =
				context.GetAimDirection();
			const int projectile_count =
				context.GetProfile().VolleyCount > 0 ?
				context.GetProfile().VolleyCount : 1;
			bool fired_any = false;
			for (int i = 0; i < projectile_count; ++i)
			{
				cProjectileDesc desc = context.BuildProjectile(
					spawn_position, aim_direction);
				desc.BezierCurveDirection = m_SpawnSide;
				desc.MagicBladeSideOffset *=
					1.0f + static_cast<float>(i / 2) * 0.35f;
				desc.Position = {
					spawn_position.x - aim_direction.x * SUMMON_BACK_OFFSET,
					spawn_position.y - aim_direction.y * SUMMON_BACK_OFFSET,
				};
				m_SpawnSide = -m_SpawnSide;
				if (!context.SubmitProjectile(desc))
				{
					continue;
				}

				fired_any = true;
				cGameEffectManager::GetInstance().Play(
					GameEffectType::PixelMagicHit,
					desc.Position,
					0.38f,
					{ 0.08f, 0.62f, 1.0f, 0.88f },
					desc.Rotation);
			}

			if (fired_any)
			{
				WeaponAudio::PlayMagicBladeSummon();
			}
			return fired_any;
		}

	private:
		float m_SpawnSide{ 1.0f };
	};
}

namespace WeaponMagicBlade
{
std::unique_ptr<IWeaponFireStrategy> CreateFireStrategy()
{
	return std::make_unique<MagicBladeFireStrategy>();
}
}
