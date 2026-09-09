#include "game_enemy_internal.h"
#include "Constants/player_constants.h"
#include "Constants/enemy_constants.h"
#include "random_utils.h"

namespace GameEnemy::Internal
{
	// 피격, 사망, 보스 처치에 필요한 시각 효과와 이벤트를 처리한다.
	void GetMonsterFrameRegion(MonsterType type, float animation_elapsed, int& frame_x, int& frame_y)
	{
		const MonsterData& data = GetMonsterData(type);
		const int animation_frame = static_cast<int>(animation_elapsed / data.AnimationSpeed) % data.FrameCount;
		frame_x = animation_frame * data.FrameWidth;
		frame_y = 0;
	}

	void SpawnCutDust(const EnemySlot& slot, int frame_x, int frame_y, const DirectX::XMFLOAT2& slash_direction)
	{
		if (g_EnemySystem.CutDustParticles.size() >= CUT_DUST_CAPACITY)
		{
			return;
		}
		const std::size_t type_index = MonsterTypeToIndex(slot.Type);
		const DirectX::XMUINT2 texture_size = g_EnemySystem.MonsterTextureSizes[type_index];
		if (texture_size.x == 0 || texture_size.y == 0)
		{
			return;
		}
		const MonsterData& data = GetMonsterData(slot.Type);
		const bool flip_horizontal = slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
		const float draw_width = (flip_horizontal ? -data.DrawSize.x : data.DrawSize.x) * slot.DrawScale;
		const float draw_height = data.DrawSize.y * slot.DrawScale;
		const DirectX::XMFLOAT2 direction = NormalizeOr(slash_direction, { 1.0f, 0.0f });
		const DirectX::XMFLOAT2 normal{ -direction.y, direction.x };
		const DirectX::XMFLOAT2 frame_uv_offset{
			static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
			static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
		};
		const DirectX::XMFLOAT2 cell_uv_scale{
			static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x * CUT_DUST_GRID_SIZE),
			static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y * CUT_DUST_GRID_SIZE),
		};
		const float cell_width = std::abs(draw_width) / CUT_DUST_GRID_SIZE;
		const float cell_height = draw_height / CUT_DUST_GRID_SIZE;
		const int requested_particle_count = std::clamp(static_cast<int>((std::abs(draw_width) + draw_height) * 0.28f),
		                                                CUT_DUST_MIN_PARTICLE_COUNT, CUT_DUST_MAX_PARTICLE_COUNT);
		for (int i = 0; i < requested_particle_count && g_EnemySystem.CutDustParticles.size() < CUT_DUST_CAPACITY; ++i)
		{
			const int cell_x = std::min(CUT_DUST_GRID_SIZE - 1, static_cast<int>(Random01() * CUT_DUST_GRID_SIZE));
			const int cell_y = std::min(CUT_DUST_GRID_SIZE - 1, static_cast<int>(Random01() * CUT_DUST_GRID_SIZE));
			const float normalized_x = (static_cast<float>(cell_x) + 0.5f) / CUT_DUST_GRID_SIZE - 0.5f;
			const float normalized_y = (static_cast<float>(cell_y) + 0.5f) / CUT_DUST_GRID_SIZE - 0.5f;
			const DirectX::XMFLOAT2 local_position{
				normalized_x * draw_width,
				normalized_y * draw_height,
			};
			const float side = local_position.x * normal.x + local_position.y * normal.y < 0.0f ? -1.0f : 1.0f;
			const float fragment_scale = 0.58f + Random01() * 0.56f;
			const float along_speed = 65.0f + Random01() * 80.0f;
			const float outward_speed = 80.0f + Random01() * 145.0f;
			const float tangent_jitter = RandomSigned() * 55.0f;
			CutDustParticle particle{};
			particle.Position = {
				slot.Entity.GetPosition().x + local_position.x + RandomSigned() * cell_width * 0.24f,
				slot.Entity.GetPosition().y + local_position.y + RandomSigned() * cell_height * 0.24f,
			};
			particle.Velocity = {
				direction.x * (along_speed + tangent_jitter) + normal.x * side * outward_speed,
				direction.y * (along_speed + tangent_jitter) + normal.y * side * outward_speed -
				    (28.0f + Random01() * 54.0f),
			};
			particle.Size = {
				std::max(2.0f, cell_width * fragment_scale),
				std::max(2.0f, cell_height * fragment_scale),
			};
			particle.TexcoordScale = {
				flip_horizontal ? -cell_uv_scale.x : cell_uv_scale.x,
				cell_uv_scale.y,
			};
			particle.TexcoordOffset = {
				frame_uv_offset.x + static_cast<float>(cell_x + (flip_horizontal ? 1 : 0)) * cell_uv_scale.x,
				frame_uv_offset.y + static_cast<float>(cell_y) * cell_uv_scale.y,
			};
			particle.Rotation = Random01() * DirectX::XM_2PI;
			particle.AngularVelocity = RandomSigned() * 13.0f;
			particle.Lifetime = 0.44f + Random01() * 0.34f;
			particle.Type = slot.Type;
			g_EnemySystem.CutDustParticles.push_back(particle);
		}
	}

	void BeginDirectionalCutDeath(int enemy_id, EnemySlot& slot, const DirectX::XMFLOAT2& slash_direction)
	{
		int frame_x = 0;
		int frame_y = 0;
		const float animation_elapsed =
		    EnemyAttackPattern::GetAnimationElapsed(enemy_id, g_EnemySystem.MonsterAnimationElapsed);
		GetMonsterFrameRegion(slot.Type, animation_elapsed, frame_x, frame_y);
		EnemyAttackPattern::GetAnimationFrameRegion(enemy_id, frame_x, frame_y);
		slot.Entity.BeginCutDeath(slash_direction, frame_x, frame_y);
		SpawnCutDust(slot, frame_x, frame_y, slash_direction);
	}

	void SpawnBoneDrop(const DirectX::XMFLOAT2& position)
	{
		for (int i = 0; i < BONE_BURST_PARTICLE_COUNT; ++i)
		{
			const float angle = Random01() * DirectX::XM_2PI;
			const float speed = 130.0f + Random01() * 150.0f;
			BoneDropEffect bone{};
			bone.Position = {
				position.x + RandomSigned() * 8.0f,
				position.y + RandomSigned() * 10.0f,
			};
			bone.Velocity = {
				std::cos(angle) * speed,
				std::sin(angle) * speed - 55.0f,
			};
			bone.Size = 24.0f + Random01() * 10.0f;
			bone.Rotation = Random01() * DirectX::XM_2PI;
			bone.AngularVelocity = RandomSigned() * 12.0f;
			bone.Lifetime = 0.72f + Random01() * 0.38f;
			bone.Frame = i % BONE_DROP_FRAME_COUNT;
			g_EnemySystem.BoneDropEffects.push_back(bone);
		}
	}

	void PlayDamageReaction(MonsterType type, const DirectX::XMFLOAT2& position,
	                        const DirectX::XMFLOAT2& impact_direction, bool was_killed)
	{
		if (type == MonsterType::Bat)
		{
			const int bat_audio_id = was_killed ? g_EnemySystem.BatDeathAudioID : g_EnemySystem.BatDamageAudioID;
			if (bat_audio_id >= 0)
			{
				Audio_Play(bat_audio_id);
			}
		}
		else if (IsOrcType(type))
		{
			if (was_killed)
			{
				if (g_EnemySystem.OrcDeathAudioID >= 0)
				{
					Audio_Play(g_EnemySystem.OrcDeathAudioID);
				}
			}
			else
			{
				const int sound_index = RandomInt(0, static_cast<int>(g_EnemySystem.OrcDamageAudioIDs.size()) - 1);
				if (g_EnemySystem.OrcDamageAudioIDs[sound_index] >= 0)
				{
					Audio_Play(g_EnemySystem.OrcDamageAudioIDs[sound_index]);
				}
			}
		}
		const MonsterDeathEffect death_effect = GetMonsterData(type).DeathEffect;
		if (death_effect == MonsterDeathEffect::Slime)
		{
			const int slime_audio_id = was_killed ? g_EnemySystem.SlimeDeathAudioID : g_EnemySystem.SlimeDamageAudioID;
			if (slime_audio_id >= 0)
			{
				Audio_Play(slime_audio_id);
			}
			cSlimeGoo::GetInstance().Spawn(position, impact_direction, was_killed ? 1.65f : 1.0f);
			return;
		}
		if (death_effect == MonsterDeathEffect::Bones)
		{
			const int bones_audio_id = was_killed ? g_EnemySystem.BonesDeathAudioID : g_EnemySystem.BonesDamageAudioID;
			if (bones_audio_id >= 0)
			{
				Audio_Play(bones_audio_id);
			}
			if (was_killed)
			{
				SpawnBoneDrop(position);
			}
			return;
		}
		Blood::Spawn(position);
		if (was_killed)
		{
			// 사망 시 혈흔 효과를 생성한다.
			Blood::Spawn(position);
		}
	}

	void QueueBossDefeatedEvent(const EnemySlot& defeated_slot)
	{
		if (!IsBossType(defeated_slot.Type) ||
		    std::any_of(std::begin(g_EnemySystem.EnemySlots), std::end(g_EnemySystem.EnemySlots),
		                [](const EnemySlot& slot)
		                {
			                return IsBossType(slot.Type) && slot.Entity.IsAlive();
		                }))
		{
			return;
		}
		const MonsterData& data = GetMonsterData(defeated_slot.Type);
		g_EnemySystem.PendingBossDefeat.Position = defeated_slot.Entity.GetPosition();
		// 보스 사망 연출에 사용할 원래 표시 크기를 저장한다.
		g_EnemySystem.PendingBossDefeat.DrawSize = data.DrawSize;
		g_EnemySystem.BossDefeatedEventPending = true;
	}

	void HandleEnemyDefeat(EnemySlot& slot, const DirectX::XMFLOAT2& effect_position)
	{
		static constexpr int Count = 10;
		cGameEffectManager::GetInstance().PlayEnemyDefeat(effect_position);
		for (int gem_index = 0; gem_index < Count; ++gem_index)
		{
			GameExperienceGem::Spawn(slot.Entity.GetPosition(), GetEnemyExperienceDrop(slot), slot.RoomIndex);
		}
		GameHealingItem::TrySpawn(slot.Entity.GetPosition(), slot.RoomIndex);
		QueueBossDefeatedEvent(slot);
	}

	void PlayDashSlashImpact(const DirectX::XMFLOAT2& position, const DirectX::XMFLOAT2& direction, int hit_index)
	{
		static constexpr std::array<float, PlayerConstants::Slash::CutCount> CutScales{
			0.62f, 0.68f, 0.74f, 0.82f, 0.94f,
		};
		static constexpr std::array<float, PlayerConstants::Slash::CutCount> BurstScales{
			0.72f, 0.8f, 0.88f, 0.98f, 1.18f,
		};
		static constexpr std::array<float, PlayerConstants::Slash::CutCount> NormalOffsets{
			-8.0f, 8.0f, -5.0f, 5.0f, 0.0f,
		};
		static constexpr std::array<float, PlayerConstants::Slash::CutCount> CrossAngles{
			0.58f, 0.72f, 0.88f, 0.68f, DirectX::XM_PIDIV4,
		};
		const int index = std::clamp(hit_index, 0, PlayerConstants::Slash::CutCount - 1);
		const float base_angle = std::atan2(direction.y, direction.x);
		const DirectX::XMFLOAT2 normal{ -direction.y, direction.x };
		const DirectX::XMFLOAT2 impact_center{
			position.x + normal.x * NormalOffsets[index],
			position.y + normal.y * NormalOffsets[index],
		};
		cGameEffectManager& effects = cGameEffectManager::GetInstance();
		const bool is_finisher = index == PlayerConstants::Slash::CutCount - 1;
		const DirectX::XMFLOAT4 burst_color = is_finisher
		                                          ? DirectX::XMFLOAT4{ 0.94f, 0.99f, 1.0f, 1.0f }
		                                          : (index % 2 == 0 ? DirectX::XMFLOAT4{ 0.34f, 0.92f, 1.0f, 0.96f }
		                                                            : DirectX::XMFLOAT4{ 0.68f, 0.32f, 1.0f, 0.96f });
		effects.Play(GameEffectType::DashSlashHitBurst, impact_center, BurstScales[index], burst_color,
		             base_angle + static_cast<float>(index) * 0.31f);
		// 짧은 두 호를 교차시켜 방향이 바뀐 연속 타격이 서로 구분되게 한다.
		for (int side = -1; side <= 1; side += 2)
		{
			const DirectX::XMFLOAT2 cut_position{
				impact_center.x + normal.x * static_cast<float>(side) * 4.0f,
				impact_center.y + normal.y * static_cast<float>(side) * 4.0f,
			};
			effects.Play(GameEffectType::DashSlashHitCut, cut_position, CutScales[index],
			             { 1.0f, 1.0f, 1.0f, side < 0 ? 0.82f : 1.0f },
			             base_angle + CrossAngles[index] * static_cast<float>(side));
		}
		if (is_finisher)
		{
			for (int quarter_turn = 0; quarter_turn < 2; ++quarter_turn)
			{
				effects.Play(GameEffectType::DashSlashHitCut, position, 1.08f, { 0.86f, 0.96f, 1.0f, 0.94f },
				             base_angle + DirectX::XM_PIDIV2 * static_cast<float>(quarter_turn));
			}
		}
	}

	void ApplyDashSlashPulse(PendingDashSlashAttack& attack)
	{
		static constexpr float DashSlashKnockbackSpeed = 240.0f;
		const int hit_index = std::clamp(attack.NextHitIndex, 0, PlayerConstants::Slash::CutCount - 1);
		if (g_EnemySystem.DashSlashHitAudioIDs[hit_index] >= 0)
		{
			Audio_Play(g_EnemySystem.DashSlashHitAudioIDs[hit_index]);
		}
		for (PendingDashSlashTarget& target : attack.Targets)
		{
			const bool target_alive =
			    IsValidEnemyID(target.EnemyID) && g_EnemySystem.EnemySlots[target.EnemyID].Entity.IsAlive();
			if (target_alive)
			{
				target.LastPosition = GetEnemyAimPosition(g_EnemySystem.EnemySlots[target.EnemyID]);
			}
			GameDamageText::Spawn(attack.Damage, target.LastPosition);
			if (target_alive)
			{
				EnemySlot& slot = g_EnemySystem.EnemySlots[target.EnemyID];
				ApplyCombatKnockback(slot, attack.Direction, DashSlashKnockbackSpeed);
				slot.Entity.ApplyDamage(attack.Damage);
				const bool was_killed = !slot.Entity.IsAlive();
				RecordCombatFeedback(slot, target.LastPosition, attack.Direction, attack.Damage, was_killed,
				                     hit_index == PlayerConstants::Slash::CutCount - 1);
				PlayDamageReaction(slot.Type, target.LastPosition, attack.Direction, was_killed);
				if (was_killed && !IsBossType(slot.Type))
				{
					BeginDirectionalCutDeath(target.EnemyID, slot, attack.Direction);
				}
				if (was_killed && !target.RewardGranted)
				{
					target.RewardGranted = true;
					HandleEnemyDefeat(slot, target.LastPosition);
				}
			}
			PlayDashSlashImpact(target.LastPosition, attack.Direction, attack.NextHitIndex);
		}
	}
} // namespace GameEnemy::Internal
