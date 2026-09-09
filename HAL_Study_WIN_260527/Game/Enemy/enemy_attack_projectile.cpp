#include "enemy_attack_pattern_internal.h"

#include "math_utils.h"
#include "game_enemy.h"
#include "Constants/enemy_pattern_constants.h"

namespace
{
	namespace EnemyPatternTuning::BoneProjectile
	{
		constexpr float MaxDistance = 900.0f;
		constexpr float Damage = 8.0f;
		constexpr float Radius = 12.0f;
	} // namespace EnemyPatternTuning::BoneProjectile

	namespace EnemyPatternTuning::MageProjectile
	{
		constexpr float Damage = 7.0f;
		constexpr float Size = 68.0f;
	} // namespace EnemyPatternTuning::MageProjectile

	namespace EnemyPatternTuning::Projectile
	{
		constexpr DirectX::XMFLOAT4 OutlineColor{ 1.0f, 0.035f, 0.015f, 0.96f };
		constexpr float OutlineThickness = 1.0f;
	} // namespace EnemyPatternTuning::Projectile

} // namespace

namespace EnemyAttackPattern
{
	using namespace Internal;

	// 적 발사체의 이동과 수명 갱신을 한곳에서 처리한다.
	void UpdateProjectiles(float delta_time)
	{
		const float safe_delta_time = std::max(delta_time, 0.0f);
		// 뼈와 단검은 회전하면서 직선 이동한다.
		for (BoneProjectile& projectile : g_BoneProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			const DirectX::XMFLOAT2 previous_position = projectile.Position;
			const DirectX::XMFLOAT2 next_position{
				previous_position.x + projectile.Velocity.x * safe_delta_time,
				previous_position.y + projectile.Velocity.y * safe_delta_time,
			};
			const float step_x = next_position.x - previous_position.x;
			const float step_y = next_position.y - previous_position.y;
			const float step_distance = Length({ step_x, step_y });
			if (!ProceduralMap_IsSegmentWalkable(previous_position, next_position,
			                                     EnemyPatternTuning::BoneProjectile::Radius) ||
			    projectile.Travelled + step_distance >= EnemyPatternTuning::BoneProjectile::MaxDistance)
			{
				projectile.IsActive = false;
				continue;
			}
			projectile.Position = next_position;
			projectile.Travelled += step_distance;
			projectile.Rotation += projectile.AngularVelocity * safe_delta_time;
		}
		for (DaggerProjectile& projectile : g_DaggerProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			const DirectX::XMFLOAT2 previous_position = projectile.Position;
			const DirectX::XMFLOAT2 next_position{
				previous_position.x + projectile.Velocity.x * safe_delta_time,
				previous_position.y + projectile.Velocity.y * safe_delta_time,
			};
			const float step_x = next_position.x - previous_position.x;
			const float step_y = next_position.y - previous_position.y;
			const float step_distance = Length({ step_x, step_y });
			if (!ProceduralMap_IsSegmentWalkable(previous_position, next_position, projectile.Radius) ||
			    projectile.Travelled + step_distance >= projectile.MaxDistance)
			{
				projectile.IsActive = false;
				continue;
			}
			projectile.Position = next_position;
			projectile.Travelled += step_distance;
			projectile.Rotation += projectile.AngularVelocity * safe_delta_time;
		}
		// 마법탄은 직선으로 이동하면서 스프라이트 애니메이션 시간을 갱신한다.
		for (MageProjectile& projectile : g_MageProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			const DirectX::XMFLOAT2 previous_position = projectile.Position;
			const DirectX::XMFLOAT2 next_position{
				previous_position.x + projectile.Velocity.x * safe_delta_time,
				previous_position.y + projectile.Velocity.y * safe_delta_time,
			};
			const float step_x = next_position.x - previous_position.x;
			const float step_y = next_position.y - previous_position.y;
			const float step_distance = Length({ step_x, step_y });
			if (!ProceduralMap_IsSegmentWalkable(previous_position, next_position,
			                                     EnemyPatternConstants::MageProjectile::Radius) ||
			    projectile.Travelled + step_distance >= EnemyPatternConstants::MageProjectile::MaxDistance)
			{
				projectile.IsActive = false;
				continue;
			}
			projectile.Position = next_position;
			projectile.Travelled += step_distance;
			projectile.AnimationElapsed += safe_delta_time;
		}
		// 전사 근접 공격은 이동하지 않고 수명만 갱신한다.
		for (WarriorStrike& strike : g_WarriorStrikes)
		{
			if (!strike.IsActive)
			{
				continue;
			}
			strike.Elapsed += safe_delta_time;
			if (strike.Elapsed >= strike.Lifetime)
			{
				strike.IsActive = false;
			}
		}
		for (GroundHazard& hazard : g_GroundHazards)
		{
			if (!hazard.IsActive)
			{
				continue;
			}
			hazard.Elapsed += safe_delta_time;
			if (hazard.Elapsed >= hazard.Lifetime)
			{
				hazard.IsActive = false;
			}
		}
	}

	void DrawTelegraphs()
	{
		// 공격 전 경고 표시를 스프라이트 인스턴스로 모아 그린다.
		static std::vector<SpriteInstance> line_instances;
		line_instances.clear();
		if (line_instances.capacity() < GameEnemy::ENEMY_CAPACITY)
		{
			line_instances.reserve(GameEnemy::ENEMY_CAPACITY);
		}
		if (g_WarningTextureID != TEXTURE_INVALID_ID)
		{
			for (const EnemyPatternRuntime& runtime : g_EnemyPatterns)
			{
				const bool slime_telegraph = runtime.State == ActionState::SlimeTelegraph;
				const bool bat_telegraph = runtime.State == ActionState::BatWindup;
				const bool orc_telegraph = runtime.State == ActionState::OrcWindup;
				const bool skeleton_rogue_telegraph = runtime.State == ActionState::SkeletonRogueWindup;
				const bool orc_rogue_telegraph = runtime.State == ActionState::RogueWindup;
				const bool mage_telegraph = runtime.State == ActionState::SkeletonMageWindup;
				const bool warrior_telegraph = runtime.State == ActionState::WarriorWindup;
				const bool cthulhu_telegraph = runtime.State == ActionState::CthulhuDashWindup;
				if (!runtime.IsActive ||
				    (!slime_telegraph && !bat_telegraph && !orc_telegraph && !skeleton_rogue_telegraph &&
				     !orc_rogue_telegraph && !mage_telegraph && !warrior_telegraph && !cthulhu_telegraph))
				{
					continue;
				}
				if (skeleton_rogue_telegraph)
				{
					const float progress =
					    1.0f - Saturate(runtime.Timer / EnemyPatternConstants::SkeletonRogue::WindupDuration);
					for (int dagger_index = -1; dagger_index <= 1; ++dagger_index)
					{
						const DirectX::XMFLOAT2 direction = RotateDirection(
						    runtime.LockedDirection,
						    static_cast<float>(dagger_index) * EnemyPatternConstants::SkeletonRogue::DaggerAngleStep);
						const DirectX::XMFLOAT2 end = ProceduralMap_TraceWalkableSegment(
						    runtime.TelegraphStart, direction, EnemyPatternConstants::DaggerProjectile::MaxDistance,
						    EnemyPatternConstants::DaggerProjectile::Radius);
						const float dx = end.x - runtime.TelegraphStart.x;
						const float dy = end.y - runtime.TelegraphStart.y;
						const float length = Length({ dx, dy });
						if (length <= 0.001f)
						{
							continue;
						}
						line_instances.push_back({
						    {
						        (runtime.TelegraphStart.x + end.x) * 0.5f,
						        (runtime.TelegraphStart.y + end.y) * 0.5f,
						    },
						    { length, 11.0f },
						    std::atan2(dy, dx),
						    { 1.0f, 0.04f, 0.01f, 0.12f + progress * 0.18f },
						});
					}
					continue;
				}
				const float dx = runtime.TelegraphEnd.x - runtime.TelegraphStart.x;
				const float dy = runtime.TelegraphEnd.y - runtime.TelegraphStart.y;
				const float length_squared = LengthSquared({ dx, dy });
				if (length_squared <= 0.0001f)
				{
					continue;
				}
				const float length = std::sqrt(length_squared);
				const float telegraph_duration =
				    slime_telegraph
				        ? EnemyPatternConstants::Slime::DashTelegraphDuration
				        : (mage_telegraph
				               ? EnemyPatternConstants::SkeletonMage::CastDuration
				               : (warrior_telegraph
				                      ? EnemyPatternConstants::Warrior::AttackWindupDuration
				                      : (orc_rogue_telegraph
				                             ? EnemyPatternConstants::Rogue::WindupDuration
				                             : (orc_telegraph
				                                    ? EnemyPatternConstants::OrcAxe::ThrowWindupDuration
				                                    : (cthulhu_telegraph
				                                           ? EnemyPatternConstants::Cthulhu::DashWindupDuration
				                                           : EnemyPatternConstants::Bat::DashWindupDuration)))));
				const float progress = 1.0f - Saturate(runtime.Timer / telegraph_duration);
				const float pulse = 0.68f + 0.32f * std::abs(std::sin(progress * DirectX::XM_PI * 5.0f));
				const float alpha =
				    cthulhu_telegraph ? 0.16f + progress * 0.30f
				    : (warrior_telegraph || orc_rogue_telegraph)
				        ? 0.10f + progress * 0.20f
				        : (mage_telegraph ? 0.10f + progress * 0.22f
				                          : (slime_telegraph ? 0.16f + progress * 0.18f : 0.12f + progress * 0.14f));
				line_instances.push_back({
				    {
				        (runtime.TelegraphStart.x + runtime.TelegraphEnd.x) * 0.5f,
				        (runtime.TelegraphStart.y + runtime.TelegraphEnd.y) * 0.5f,
				    },
				    { length, runtime.TelegraphWidth },
				    std::atan2(dy, dx),
				    { 1.0f, 0.06f, 0.02f, alpha * pulse },
				});
			}
		}
		if (!line_instances.empty())
		{
			SpriteInstanced_DrawUnlit(g_WarningTextureID, line_instances.data(),
			                          static_cast<int>(line_instances.size()));
		}
		if (g_GroundEffectTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		static std::vector<SpriteInstance> ground_instances;
		static std::vector<SpriteInstance> shaman_ground_instances;
		ground_instances.clear();
		shaman_ground_instances.clear();
		if (ground_instances.capacity() < GameEnemy::ENEMY_CAPACITY)
		{
			ground_instances.reserve(GameEnemy::ENEMY_CAPACITY);
			shaman_ground_instances.reserve(GameEnemy::ENEMY_CAPACITY);
		}
		for (const EnemyPatternRuntime& runtime : g_EnemyPatterns)
		{
			const bool shaman_cast = runtime.State == ActionState::ShamanCast;
			const bool mage_cast = runtime.State == ActionState::SkeletonMageWindup;
			if (!runtime.IsActive || (!shaman_cast && !mage_cast))
			{
				continue;
			}
			const float cast_duration = mage_cast ? EnemyPatternConstants::SkeletonMage::CastDuration
			                                      : EnemyPatternConstants::Shaman::CastDuration;
			const float progress = 1.0f - Saturate(runtime.Timer / cast_duration);
			const float pulse = 0.86f + 0.14f * std::abs(std::sin(progress * DirectX::XM_PI * 7.0f));
			const float base_size = mage_cast ? 112.0f : EnemyPatternConstants::Shaman::HazardRadius * 2.0f;
			const float size = base_size * (0.86f + progress * 0.14f);
			const SpriteInstance ground_instance{
				runtime.TargetPosition,
				{ size, size },
				progress * 1.8f,
				{ 1.0f, 0.04f, 0.01f, (0.42f + progress * 0.30f) * pulse },
			};
			ground_instances.push_back(ground_instance);
			if (shaman_cast)
			{
				shaman_ground_instances.push_back(ground_instance);
			}
		}
		if (!ground_instances.empty())
		{
			if (!shaman_ground_instances.empty())
			{
				SpriteInstanced_DrawOutlinedUnlit(g_GroundEffectTextureID, shaman_ground_instances.data(),
				                                  static_cast<int>(shaman_ground_instances.size()),
				                                  EnemyPatternTuning::Projectile::OutlineColor,
				                                  EnemyPatternTuning::Projectile::OutlineThickness);
			}
			SpriteInstanced_DrawAdditiveUnlit(g_GroundEffectTextureID, ground_instances.data(),
			                                  static_cast<int>(ground_instances.size()));
		}
	}

	void DrawProjectiles()
	{
		static constexpr int FrameColumns = 2;
		static constexpr int FrameCount = 4;
		static constexpr float FrameTime = 0.075f;
		static constexpr float Height = 24.0f;
		static constexpr float Width = 48.0f;
		static std::vector<SpriteInstance> bone_instances;
		bone_instances.clear();
		if (bone_instances.capacity() < EnemyPatternConstants::BoneProjectile::Capacity)
		{
			bone_instances.reserve(EnemyPatternConstants::BoneProjectile::Capacity);
		}
		if (g_BoneTextureID != TEXTURE_INVALID_ID)
		{
			for (const BoneProjectile& projectile : g_BoneProjectiles)
			{
				if (!projectile.IsActive)
				{
					continue;
				}
				bone_instances.push_back({
				    projectile.Position,
				    { Width, Height },
				    projectile.Rotation,
				    { 1.0f, 0.96f, 0.82f, 1.0f },
				    { 0.0f, 0.0f },
				    { 1.0f / 3.0f, 1.0f },
				});
			}
		}
		if (!bone_instances.empty())
		{
			SpriteInstanced_DrawOutlinedUnlit(
			    g_BoneTextureID, bone_instances.data(), static_cast<int>(bone_instances.size()),
			    EnemyPatternTuning::Projectile::OutlineColor, EnemyPatternTuning::Projectile::OutlineThickness);
		}
		static std::vector<SpriteInstance> dagger_instances;
		static std::vector<SpriteInstance> axe_instances;
		dagger_instances.clear();
		axe_instances.clear();
		if (dagger_instances.capacity() < EnemyPatternConstants::DaggerProjectile::Capacity)
		{
			dagger_instances.reserve(EnemyPatternConstants::DaggerProjectile::Capacity);
			axe_instances.reserve(EnemyPatternConstants::DaggerProjectile::Capacity);
		}
		for (const DaggerProjectile& projectile : g_DaggerProjectiles)
		{
			if (!projectile.IsActive)
			{
				continue;
			}
			std::vector<SpriteInstance>& instances = projectile.IsAxe ? axe_instances : dagger_instances;
			instances.push_back({
			    projectile.Position,
			    { projectile.Size, projectile.Size },
			    projectile.Rotation,
			    projectile.IsAxe ? DirectX::XMFLOAT4{ 1.0f, 0.82f, 0.62f, 1.0f }
			                     : DirectX::XMFLOAT4{ 1.0f, 0.90f, 0.78f, 1.0f },
			});
		}
		if (!dagger_instances.empty() && g_DaggerTextureID != TEXTURE_INVALID_ID)
		{
			SpriteInstanced_DrawOutlinedUnlit(
			    g_DaggerTextureID, dagger_instances.data(), static_cast<int>(dagger_instances.size()),
			    EnemyPatternTuning::Projectile::OutlineColor, EnemyPatternTuning::Projectile::OutlineThickness);
		}
		if (!axe_instances.empty() && g_AxeTextureID != TEXTURE_INVALID_ID)
		{
			SpriteInstanced_DrawOutlinedUnlit(
			    g_AxeTextureID, axe_instances.data(), static_cast<int>(axe_instances.size()),
			    EnemyPatternTuning::Projectile::OutlineColor, EnemyPatternTuning::Projectile::OutlineThickness);
		}
		static std::vector<SpriteInstance> mage_projectile_instances;
		mage_projectile_instances.clear();
		if (mage_projectile_instances.capacity() < EnemyPatternConstants::MageProjectile::Capacity)
		{
			mage_projectile_instances.reserve(EnemyPatternConstants::MageProjectile::Capacity);
		}
		if (g_MageProjectileTextureID != TEXTURE_INVALID_ID)
		{
			for (const MageProjectile& projectile : g_MageProjectiles)
			{
				if (!projectile.IsActive)
				{
					continue;
				}
				const int frame = static_cast<int>(projectile.AnimationElapsed / FrameTime) % FrameCount;
				const int frame_column = frame % FrameColumns;
				const int frame_row = frame / FrameColumns;
				mage_projectile_instances.push_back({
				    projectile.Position,
				    { EnemyPatternTuning::MageProjectile::Size, EnemyPatternTuning::MageProjectile::Size },
				    projectile.Rotation,
				    { 1.0f, 1.0f, 1.0f, 1.0f },
				    {
				        static_cast<float>(frame_column) / static_cast<float>(FrameColumns),
				        static_cast<float>(frame_row) / 2.0f,
				    },
				    { 0.5f, 0.5f },
				});
			}
		}
		if (!mage_projectile_instances.empty())
		{
			SpriteInstanced_DrawOutlinedUnlit(g_MageProjectileTextureID, mage_projectile_instances.data(),
			                                  static_cast<int>(mage_projectile_instances.size()),
			                                  EnemyPatternTuning::Projectile::OutlineColor,
			                                  EnemyPatternTuning::Projectile::OutlineThickness);
		}
		if (g_GroundEffectTextureID == TEXTURE_INVALID_ID)
		{
			return;
		}
		static std::vector<SpriteInstance> hazard_instances;
		hazard_instances.clear();
		if (hazard_instances.capacity() < EnemyPatternConstants::GroundHazard::Capacity)
		{
			hazard_instances.reserve(EnemyPatternConstants::GroundHazard::Capacity);
		}
		for (const GroundHazard& hazard : g_GroundHazards)
		{
			if (!hazard.IsActive)
			{
				continue;
			}
			const float progress = Saturate(hazard.Elapsed / hazard.Lifetime);
			const float size = hazard.Radius * 2.0f * (1.0f + progress * 0.22f);
			hazard_instances.push_back({
			    hazard.Position,
			    { size, size },
			    progress * 2.6f,
			    { 1.0f, 0.18f, 0.01f, (1.0f - progress) * 0.92f },
			});
		}
		if (!hazard_instances.empty())
		{
			SpriteInstanced_DrawAdditiveUnlit(g_GroundEffectTextureID, hazard_instances.data(),
			                                  static_cast<int>(hazard_instances.size()));
		}
	}

	void RegisterProjectileColliders(int owner_id_offset)
	{
		for (int projectile_id = 0; projectile_id < EnemyPatternConstants::BoneProjectile::Capacity; ++projectile_id)
		{
			const BoneProjectile& projectile = g_BoneProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(owner_id_offset + projectile_id, CollisionLayer::EnemyBullet,
			                               CollisionLayer::Player, projectile.Position,
			                               EnemyPatternTuning::BoneProjectile::Radius);
		}
		for (int projectile_id = 0; projectile_id < EnemyPatternConstants::DaggerProjectile::Capacity; ++projectile_id)
		{
			const DaggerProjectile& projectile = g_DaggerProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(
			    owner_id_offset + EnemyPatternConstants::BoneProjectile::Capacity + projectile_id,
			    CollisionLayer::EnemyBullet, CollisionLayer::Player, projectile.Position, projectile.Radius);
		}
		for (int projectile_id = 0; projectile_id < EnemyPatternConstants::MageProjectile::Capacity; ++projectile_id)
		{
			const MageProjectile& projectile = g_MageProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(owner_id_offset + EnemyPatternConstants::BoneProjectile::Capacity +
			                                   EnemyPatternConstants::DaggerProjectile::Capacity + projectile_id,
			                               CollisionLayer::EnemyBullet, CollisionLayer::Player, projectile.Position,
			                               EnemyPatternConstants::MageProjectile::Radius);
		}
		for (int strike_id = 0; strike_id < EnemyPatternConstants::Warrior::StrikeCapacity; ++strike_id)
		{
			const WarriorStrike& strike = g_WarriorStrikes[strike_id];
			if (!strike.IsActive)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(owner_id_offset + EnemyPatternConstants::BoneProjectile::Capacity +
			                                   EnemyPatternConstants::DaggerProjectile::Capacity +
			                                   EnemyPatternConstants::MageProjectile::Capacity + strike_id,
			                               CollisionLayer::EnemyBullet, CollisionLayer::Player, strike.Position,
			                               strike.Radius);
		}
		for (int hazard_id = 0; hazard_id < EnemyPatternConstants::GroundHazard::Capacity; ++hazard_id)
		{
			const GroundHazard& hazard = g_GroundHazards[hazard_id];
			if (!hazard.IsActive)
			{
				continue;
			}
			CollisionSystem_RegisterCircle(owner_id_offset + EnemyPatternConstants::BoneProjectile::Capacity +
			                                   EnemyPatternConstants::DaggerProjectile::Capacity +
			                                   EnemyPatternConstants::MageProjectile::Capacity +
			                                   EnemyPatternConstants::Warrior::StrikeCapacity + hazard_id,
			                               CollisionLayer::EnemyBullet, CollisionLayer::Player, hazard.Position,
			                               hazard.Radius);
		}
	}

	bool ConsumeProjectile(int projectile_id, float& out_damage)
	{
		if (projectile_id < 0)
		{
			return false;
		}
		if (projectile_id < EnemyPatternConstants::BoneProjectile::Capacity)
		{
			BoneProjectile& projectile = g_BoneProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				return false;
			}
			out_damage = EnemyPatternTuning::BoneProjectile::Damage;
			projectile.IsActive = false;
			return true;
		}
		projectile_id -= EnemyPatternConstants::BoneProjectile::Capacity;
		if (projectile_id < EnemyPatternConstants::DaggerProjectile::Capacity)
		{
			DaggerProjectile& projectile = g_DaggerProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				return false;
			}
			out_damage = projectile.Damage;
			projectile.IsActive = false;
			return true;
		}
		projectile_id -= EnemyPatternConstants::DaggerProjectile::Capacity;
		if (projectile_id < EnemyPatternConstants::MageProjectile::Capacity)
		{
			MageProjectile& projectile = g_MageProjectiles[projectile_id];
			if (!projectile.IsActive)
			{
				return false;
			}
			out_damage = EnemyPatternTuning::MageProjectile::Damage;
			projectile.IsActive = false;
			return true;
		}
		projectile_id -= EnemyPatternConstants::MageProjectile::Capacity;
		if (projectile_id < EnemyPatternConstants::Warrior::StrikeCapacity)
		{
			WarriorStrike& strike = g_WarriorStrikes[projectile_id];
			if (!strike.IsActive)
			{
				return false;
			}
			out_damage = strike.Damage;
			strike.IsActive = false;
			return true;
		}
		projectile_id -= EnemyPatternConstants::Warrior::StrikeCapacity;
		if (projectile_id < EnemyPatternConstants::GroundHazard::Capacity)
		{
			GroundHazard& hazard = g_GroundHazards[projectile_id];
			if (!hazard.IsActive)
			{
				return false;
			}
			out_damage = hazard.Damage;
			hazard.IsActive = false;
			return true;
		}
		return false;
	}
} // namespace EnemyAttackPattern
