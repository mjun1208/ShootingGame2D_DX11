#include "game_enemy_internal.h"

#include "math_utils.h"
#include "sprite.h"
#include "game_enemy.h"
#include "Constants/enemy_constants.h"

namespace
{

	namespace EnemyTuning::CorruptionAura
	{
		constexpr float FrameWidth = 384.0f;
		constexpr float FrameHeight = 512.0f;
	} // namespace EnemyTuning::CorruptionAura

} // namespace

namespace GameEnemy
{
	using namespace Internal;

	// 적, 공격 예고, 투사체, 포인트 라이트를 그린다.
	void AppendBossTelegraphLine(std::vector<SpriteInstance>& instances, const DirectX::XMFLOAT2& start,
	                             const DirectX::XMFLOAT2& end, float width, const DirectX::XMFLOAT4& color)
	{
		const float dx = end.x - start.x;
		const float dy = end.y - start.y;
		const float length_squared = DistanceSquared(start, end);
		if (length_squared <= 0.0001f)
		{
			return;
		}
		const float length = std::sqrt(length_squared);
		instances.push_back({
		    { (start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f },
		    { length, width },
		    std::atan2(dy, dx),
		    color,
		    { 0.0f, 0.0f },
		    { 1.0f, 1.0f },
		    1.0f,
		});
	}

	void DrawBossJellyTelegraph()
	{
		static constexpr float TelegraphTraceStep = 10.0f;
		if (g_EnemySystem.BossJellyTelegraphTextureID == TEXTURE_INVALID_ID ||
		    !IsValidEnemyID(g_EnemySystem.BossJellyTelegraphEnemyID))
		{
			return;
		}
		const EnemySlot& firing_boss = g_EnemySystem.EnemySlots[g_EnemySystem.BossJellyTelegraphEnemyID];
		if (firing_boss.Type != MonsterType::BossSlime || !firing_boss.Entity.IsAlive())
		{
			return;
		}
		const float remaining =
		    std::clamp(g_EnemySystem.BossJellyFireCooldown, 0.0f, EnemyConstants::BossJelly::TelegraphDuration);
		const float progress = 1.0f - remaining / EnemyConstants::BossJelly::TelegraphDuration;
		const float pulse = 0.68f + 0.32f * std::abs(std::sin(progress * DirectX::XM_PI * 5.0f));
		static std::vector<SpriteInstance> warning_instances;
		warning_instances.clear();
		if (warning_instances.capacity() < 2)
		{
			warning_instances.reserve(2);
		}
		const bool phase_two = g_EnemySystem.BossSplitStage > 0 || firing_boss.Entity.GetHitPointRatio() <= 0.5f;
		const int shot_count = phase_two ? 2 : 1;
		for (int shot_index = 0; shot_index < shot_count; ++shot_index)
		{
			const DirectX::XMFLOAT2 direction =
			    GetBossJellyPrimaryDirection(g_EnemySystem.BossJellyTelegraphDirection, phase_two, shot_index);
			const DirectX::XMFLOAT2 muzzle_position = GetBossJellyMuzzlePosition(firing_boss, direction);
			const DirectX::XMFLOAT2 telegraph_end_position = ProceduralMap_TraceWalkableSegment(
			    muzzle_position, direction, EnemyConstants::BossJelly::PrimaryDistance, 30.0f, TelegraphTraceStep);
			AppendBossTelegraphLine(warning_instances, muzzle_position, telegraph_end_position, 60.0f,
			                        { 1.0f, 0.0f, 0.02f, (0.18f + progress * 0.12f) * pulse });
		}
		SpriteInstanced_DrawUnlit(g_EnemySystem.BossJellyTelegraphTextureID, warning_instances.data(),
		                          static_cast<int>(warning_instances.size()));
	}

	void DrawSpawnTelegraphs()
	{
		static constexpr float ArrivalRingExpansion = 34.0f;
		DrawBossJellyTelegraph();
		EnemyAttackPattern::DrawTelegraphs();
		if (g_EnemySystem.SpawnTelegraphTextureID == TEXTURE_INVALID_ID ||
		    (g_EnemySystem.PendingEnemySpawns.empty() && g_EnemySystem.SpawnArrivalEffects.empty()))
		{
			return;
		}
		static std::vector<SpriteInstance> telegraph_instances;
		if (telegraph_instances.capacity() < GameEnemy::ENEMY_CAPACITY)
		{
			telegraph_instances.reserve(GameEnemy::ENEMY_CAPACITY);
		}
		telegraph_instances.clear();
		for (const PendingEnemySpawn& pending : g_EnemySystem.PendingEnemySpawns)
		{
			const float progress = Saturate(pending.Elapsed / EnemyConstants::Spawn::TelegraphDuration);
			const float completion_flash = Saturate((progress - 0.82f) / 0.18f);
			const float size = GetSpawnTelegraphSize(pending.Type) * (0.96f + completion_flash * 0.04f);
			telegraph_instances.push_back({
			    pending.Position,
			    { size, size },
			    0.0f,
			    { 1.0f, 1.0f, 1.0f, 0.78f + completion_flash * 0.22f },
			    { 0.0f, 0.0f },
			    { 1.0f, 1.0f },
			    2.0f + progress,
			});
		}
		for (const SpawnArrivalEffect& arrival : g_EnemySystem.SpawnArrivalEffects)
		{
			const float progress = Saturate(arrival.Elapsed / EnemyConstants::Spawn::ArrivalRingDuration);
			const float smooth_progress = SmoothStep(progress);
			const float alpha = (1.0f - progress) * (1.0f - progress) * 0.9f;
			const float size = arrival.BaseSize + ArrivalRingExpansion * smooth_progress;
			telegraph_instances.push_back({
			    arrival.Position,
			    { size, size },
			    0.0f,
			    { 0.72f, 0.92f, 1.0f, alpha },
			    { 0.0f, 0.0f },
			    { 1.0f, 1.0f },
			});
		}
		SpriteInstanced_DrawAdditiveUnlit(g_EnemySystem.SpawnTelegraphTextureID, telegraph_instances.data(),
		                                  static_cast<int>(telegraph_instances.size()));
	}

	void Draw()
	{
		static constexpr int Count = 6;
		static constexpr int PathCapacity = 2;
		static constexpr float FrameDuration = 0.10f;
		static constexpr int FrameCount = 8;
		static constexpr int ColumnCount = 4;
		// 뼛조각은 몬스터보다 먼저 그려 바닥에 깔린 것처럼 보이게 한다.
		if (g_EnemySystem.BoneDropTextureID != TEXTURE_INVALID_ID && !g_EnemySystem.BoneDropEffects.empty())
		{
			static std::vector<SpriteInstance> bone_instances;
			if (bone_instances.capacity() < GameEnemy::ENEMY_CAPACITY)
			{
				bone_instances.reserve(GameEnemy::ENEMY_CAPACITY);
			}
			bone_instances.clear();
			for (const BoneDropEffect& bone_drop : g_EnemySystem.BoneDropEffects)
			{
				const float progress =
				    bone_drop.Lifetime > 0.0f ? Saturate(bone_drop.Elapsed / bone_drop.Lifetime) : 1.0f;
				const float alpha = progress < 0.55f ? 1.0f : (1.0f - progress) / 0.45f;
				const float size = bone_drop.Size * (1.0f - progress * 0.12f);
				bone_instances.push_back({
				    bone_drop.Position,
				    { size, size },
				    bone_drop.Rotation,
				    { 1.0f, 1.0f, 1.0f, alpha },
				    {
				        static_cast<float>(bone_drop.Frame * BONE_DROP_FRAME_WIDTH) / BONE_DROP_TEXTURE_WIDTH,
				        0.0f,
				    },
				    {
				        static_cast<float>(BONE_DROP_FRAME_WIDTH) / BONE_DROP_TEXTURE_WIDTH,
				        static_cast<float>(BONE_DROP_FRAME_HEIGHT) / BONE_DROP_TEXTURE_HEIGHT,
				    },
				});
			}
			SpriteInstanced_DrawUnlit(g_EnemySystem.BoneDropTextureID, bone_instances.data(),
			                          static_cast<int>(bone_instances.size()));
		}
		// 같은 텍스처를 쓰는 적끼리 모아 인스턴싱으로 한 번에 그린다.
		static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_instances;
		static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_hit_flash_instances;
		static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> monster_afterimage_instances;
		static std::array<std::vector<CutSpriteInstance>, MONSTER_TYPE_COUNT> monster_cut_instances;
		static std::array<std::vector<SpriteInstance>, MONSTER_TYPE_COUNT> cut_dust_instances;
		static std::vector<SpriteInstance> boss_split_glow_instances;
		static std::vector<SpriteInstance> corruption_aura_instances;
		static std::vector<SpriteInstance> corruption_aura_glow_instances;
		boss_split_glow_instances.clear();
		corruption_aura_instances.clear();
		corruption_aura_glow_instances.clear();
		if (boss_split_glow_instances.capacity() < 4)
		{
			boss_split_glow_instances.reserve(4);
		}
		for (std::vector<SpriteInstance>& instances : monster_instances)
		{
			if (instances.capacity() < GameEnemy::ENEMY_CAPACITY)
			{
				instances.reserve(GameEnemy::ENEMY_CAPACITY);
			}
			instances.clear();
		}
		for (std::vector<SpriteInstance>& instances : monster_hit_flash_instances)
		{
			if (instances.capacity() < GameEnemy::ENEMY_CAPACITY)
			{
				instances.reserve(GameEnemy::ENEMY_CAPACITY);
			}
			instances.clear();
		}
		for (std::vector<SpriteInstance>& instances : monster_afterimage_instances)
		{
			if (instances.capacity() < GameEnemy::ENEMY_CAPACITY)
			{
				instances.reserve(GameEnemy::ENEMY_CAPACITY);
			}
			instances.clear();
		}
		for (std::vector<CutSpriteInstance>& instances : monster_cut_instances)
		{
			if (instances.capacity() < GameEnemy::ENEMY_CAPACITY * 2)
			{
				instances.reserve(GameEnemy::ENEMY_CAPACITY * 2);
			}
			instances.clear();
		}
		for (std::vector<SpriteInstance>& instances : cut_dust_instances)
		{
			if (instances.capacity() < 256)
			{
				instances.reserve(256);
			}
			instances.clear();
		}
		// 살아 있는 적과 사망 중인 적을 각자 필요한 렌더 목록에 넣는다.
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			const EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
			int frame_x = 0;
			int frame_y = 0;
			if (slot.Entity.IsCutDeath())
			{
				frame_x = slot.Entity.GetCutFrameX();
				frame_y = slot.Entity.GetCutFrameY();
			}
			else
			{
				const float animation_elapsed =
				    EnemyAttackPattern::GetAnimationElapsed(enemy_id, g_EnemySystem.MonsterAnimationElapsed);
				GetMonsterFrameRegion(slot.Type, animation_elapsed, frame_x, frame_y);
				EnemyAttackPattern::GetAnimationFrameRegion(enemy_id, frame_x, frame_y);
			}
			const int texture_id = GetMonsterTextureID(slot.Type);
			if (slot.Entity.IsAlive())
			{
				std::vector<SpriteInstance>& instances = monster_instances[MonsterTypeToIndex(slot.Type)];
				const MonsterData& data = GetMonsterData(slot.Type);
				const DirectX::XMUINT2 texture_size = g_EnemySystem.MonsterTextureSizes[MonsterTypeToIndex(slot.Type)];
				const bool flip_horizontal = slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
				const DirectX::XMFLOAT2 fire_visual_scale = GetBossFireVisualScale(slot);
				const DirectX::XMFLOAT2 split_visual_scale = GetBossSplitVisualScale(slot);
				const float hit_visual_scale = slot.Entity.GetHitVisualScale();
				const float draw_width = (flip_horizontal ? -data.DrawSize.x : data.DrawSize.x) * slot.DrawScale *
				                         fire_visual_scale.x * split_visual_scale.x * hit_visual_scale;
				const float draw_height =
				    data.DrawSize.y * slot.DrawScale * fire_visual_scale.y * split_visual_scale.y * hit_visual_scale;
				const DirectX::XMFLOAT2 texcoord_offset{
					static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
					static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
				};
				const DirectX::XMFLOAT2 texcoord_scale{
					static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x),
					static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y),
				};
				if (slot.Type == MonsterType::OrcWarrior || slot.Type == MonsterType::OrcRogue)
				{
					EnemyAttackPattern::DashAfterimagePath paths[PathCapacity]{};
					const int path_count = EnemyAttackPattern::GetDashAfterimagePaths(enemy_id, paths, PathCapacity);
					std::vector<SpriteInstance>& afterimages =
					    monster_afterimage_instances[MonsterTypeToIndex(slot.Type)];
					for (int path_index = 0; path_index < path_count; ++path_index)
					{
						const EnemyAttackPattern::DashAfterimagePath& path = paths[path_index];
						for (int i = 0; i < Count; ++i)
						{
							const float color_amount = static_cast<float>(i) / static_cast<float>(Count - 1);
							const float path_amount = static_cast<float>(i) / static_cast<float>(Count);
							afterimages.push_back({
							    {
							        path.Start.x + (path.End.x - path.Start.x) * path_amount,
							        path.Start.y + (path.End.y - path.Start.y) * path_amount,
							    },
							    { draw_width, draw_height },
							    0.0f,
							    {
							        1.0f,
							        0.28f + 0.30f * color_amount,
							        0.12f,
							        path.Fade * (0.24f + 0.30f * color_amount),
							    },
							    texcoord_offset,
							    texcoord_scale,
							});
						}
					}
				}
				const float visual_alpha = EnemyAttackPattern::GetVisualAlpha(enemy_id);
				instances.push_back({
				    slot.Entity.GetPosition(),
				    { draw_width, draw_height },
				    0.0f,
				    { 1.0f, 1.0f, 1.0f, visual_alpha },
				    texcoord_offset,
				    texcoord_scale,
				});
				const float hit_flash = slot.Entity.GetHitFlashAmount();
				if (hit_flash > 0.001f)
				{
					SpriteInstance flash_instance = instances.back();
					flash_instance.Size.x *= 1.025f;
					flash_instance.Size.y *= 1.025f;
					flash_instance.Color = {
						1.0f,
						0.88f + hit_flash * 0.12f,
						0.48f + hit_flash * 0.52f,
						hit_flash * 0.82f * visual_alpha,
					};
					flash_instance.ColorMask = 1.0f;
					monster_hit_flash_instances[MonsterTypeToIndex(slot.Type)].push_back(flash_instance);
				}
				if (slot.Type == MonsterType::BossCorruptedKnight || slot.Type == MonsterType::BossCorruptedMage)
				{
					const int aura_frame =
					    static_cast<int>(g_EnemySystem.MonsterAnimationElapsed / FrameDuration) % FrameCount;
					const int aura_column = aura_frame % ColumnCount;
					const int aura_row = aura_frame / ColumnCount;
					const float aura_pulse = 1.0f + 0.035f * std::sin(g_EnemySystem.MonsterAnimationElapsed * 7.0f);
					const float aura_width = std::abs(draw_width) * 1.05f * aura_pulse;
					const float aura_height = draw_height * 1.48f * aura_pulse;
					const DirectX::XMFLOAT2 aura_position{
						slot.Entity.GetPosition().x,
						slot.Entity.GetPosition().y + draw_height * 0.10f,
					};
					const DirectX::XMFLOAT2 aura_uv_offset{
						static_cast<float>(aura_column) / static_cast<float>(ColumnCount),
						static_cast<float>(aura_row) / 2.0f,
					};
					const DirectX::XMFLOAT2 aura_uv_scale{
						EnemyTuning::CorruptionAura::FrameWidth / 1536.0f,
						EnemyTuning::CorruptionAura::FrameHeight / 1024.0f,
					};
					corruption_aura_instances.push_back({
					    aura_position,
					    { aura_width, aura_height },
					    0.0f,
					    { 0.66f, 0.24f, 1.0f, 0.82f * visual_alpha },
					    aura_uv_offset,
					    aura_uv_scale,
					});
					corruption_aura_glow_instances.push_back({
					    aura_position,
					    { aura_width * 1.08f, aura_height * 1.06f },
					    0.0f,
					    { 0.36f, 0.02f, 0.78f, 0.24f * visual_alpha },
					    aura_uv_offset,
					    aura_uv_scale,
					});
				}
				const float split_flash = GetBossSplitFlashAmount(slot);
				if (split_flash > 0.001f)
				{
					const float glow_scale = 1.06f + split_flash * 0.16f;
					boss_split_glow_instances.push_back({
					    slot.Entity.GetPosition(),
					    { draw_width * glow_scale, draw_height * glow_scale },
					    0.0f,
					    { 1.0f, 0.24f, 0.16f, split_flash * 0.72f },
					    texcoord_offset,
					    texcoord_scale,
					});
				}
			}
			else if (slot.Entity.IsActive())
			{
				const MonsterData& data = GetMonsterData(slot.Type);
				const std::size_t type_index = MonsterTypeToIndex(slot.Type);
				const DirectX::XMUINT2 texture_size = g_EnemySystem.MonsterTextureSizes[type_index];
				if (slot.Entity.IsCutDeath() && texture_size.x > 0 && texture_size.y > 0)
				{
					const bool flip_horizontal = slot.Entity.IsFacingLeft() != data.SourceFacesLeft;
					const float draw_width = (flip_horizontal ? -data.DrawSize.x : data.DrawSize.x) * slot.DrawScale;
					const float draw_height = data.DrawSize.y * slot.DrawScale;
					const DirectX::XMFLOAT2 texcoord_offset{
						static_cast<float>(frame_x) / static_cast<float>(texture_size.x),
						static_cast<float>(frame_y) / static_cast<float>(texture_size.y),
					};
					const DirectX::XMFLOAT2 texcoord_scale{
						static_cast<float>(data.FrameWidth) / static_cast<float>(texture_size.x),
						static_cast<float>(data.FrameHeight) / static_cast<float>(texture_size.y),
					};
					const DirectX::XMFLOAT2 direction = slot.Entity.GetCutDirection();
					const DirectX::XMFLOAT2 normal{ -direction.y, direction.x };
					const float progress = slot.Entity.GetDeathProgress();
					const float split_amount = Saturate(progress / 0.48f);
					const float smooth_split = 1.0f - std::pow(1.0f - split_amount, 3.0f);
					const float separation = std::min(std::abs(draw_width), draw_height) * 0.23f * smooth_split;
					const float dissolve_progress = Saturate((progress - 0.18f) / 0.82f);
					const float max_cut_distance =
					    0.5f * (std::abs(normal.x * draw_width) + std::abs(normal.y * draw_height));
					const float alpha = progress < 0.88f ? 1.0f : Saturate((1.0f - progress) / 0.12f);
					const float noise_seed = static_cast<float>(enemy_id * 17 + static_cast<int>(type_index) * 131);
					std::vector<CutSpriteInstance>& instances = monster_cut_instances[type_index];
					for (int side_index = -1; side_index <= 1; side_index += 2)
					{
						const float side = static_cast<float>(side_index);
						instances.push_back({
						    {
						        slot.Entity.GetPosition().x + direction.x * progress * 9.0f +
						            normal.x * side * separation,
						        slot.Entity.GetPosition().y + direction.y * progress * 9.0f +
						            normal.y * side * separation + progress * progress * 15.0f,
						    },
						    { draw_width, draw_height },
						    side * progress * 0.12f,
						    { 1.0f, 1.0f, 1.0f, alpha },
						    texcoord_offset,
						    texcoord_scale,
						    normal,
						    { side, dissolve_progress, max_cut_distance, noise_seed },
						    { 0.58f, 0.92f, 1.0f, 0.95f },
						});
					}
				}
				else
				{
					slot.Entity.Draw(texture_id, { frame_x, frame_y, data.FrameWidth, data.FrameHeight },
					                 { data.DrawSize.x * slot.DrawScale, data.DrawSize.y * slot.DrawScale },
					                 data.SourceFacesLeft);
				}
			}
		}
		// 베기 파편도 몬스터 텍스처별로 묶어서 그린다.
		for (const CutDustParticle& particle : g_EnemySystem.CutDustParticles)
		{
			const float progress = particle.Lifetime > 0.0f ? Saturate(particle.Elapsed / particle.Lifetime) : 1.0f;
			const float alpha = progress < 0.45f ? 1.0f : Saturate((1.0f - progress) / 0.55f);
			const float shrink = 1.0f - progress * 0.58f;
			cut_dust_instances[MonsterTypeToIndex(particle.Type)].push_back({
			    particle.Position,
			    { particle.Size.x * shrink, particle.Size.y * shrink },
			    particle.Rotation,
			    { 0.88f, 0.96f, 1.0f, alpha },
			    particle.TexcoordOffset,
			    particle.TexcoordScale,
			});
		}
		if (g_EnemySystem.CorruptionAuraTextureID != TEXTURE_INVALID_ID && !corruption_aura_instances.empty())
		{
			SpriteInstanced_DrawUnlit(g_EnemySystem.CorruptionAuraTextureID, corruption_aura_instances.data(),
			                          static_cast<int>(corruption_aura_instances.size()));
			SpriteInstanced_DrawAdditiveUnlit(g_EnemySystem.CorruptionAuraTextureID,
			                                  corruption_aura_glow_instances.data(),
			                                  static_cast<int>(corruption_aura_glow_instances.size()));
		}
		for (MonsterType type : MONSTER_TYPES)
		{
			std::vector<SpriteInstance>& afterimages = monster_afterimage_instances[MonsterTypeToIndex(type)];
			if (!afterimages.empty())
			{
				SpriteInstanced_DrawUnlit(GetMonsterTextureID(type), afterimages.data(),
				                          static_cast<int>(afterimages.size()));
			}
		}
		for (MonsterType type : MONSTER_TYPES)
		{
			std::vector<SpriteInstance>& instances = monster_instances[MonsterTypeToIndex(type)];
			if (!instances.empty())
			{
				SpriteInstanced_Draw(GetMonsterTextureID(type), instances.data(), static_cast<int>(instances.size()));
			}
		}
		for (MonsterType type : MONSTER_TYPES)
		{
			std::vector<SpriteInstance>& instances = monster_hit_flash_instances[MonsterTypeToIndex(type)];
			if (!instances.empty())
			{
				SpriteInstanced_DrawAdditiveUnlit(GetMonsterTextureID(type), instances.data(),
				                                  static_cast<int>(instances.size()));
			}
		}
		for (MonsterType type : MONSTER_TYPES)
		{
			std::vector<CutSpriteInstance>& instances = monster_cut_instances[MonsterTypeToIndex(type)];
			if (!instances.empty())
			{
				SpriteInstanced_DrawCut(GetMonsterTextureID(type), instances.data(),
				                        static_cast<int>(instances.size()));
			}
		}
		for (MonsterType type : MONSTER_TYPES)
		{
			std::vector<SpriteInstance>& instances = cut_dust_instances[MonsterTypeToIndex(type)];
			if (!instances.empty())
			{
				SpriteInstanced_Draw(GetMonsterTextureID(type), instances.data(), static_cast<int>(instances.size()));
			}
		}
		if (!boss_split_glow_instances.empty())
		{
			SpriteInstanced_DrawAdditiveUnlit(GetMonsterTextureID(MonsterType::BossSlime),
			                                  boss_split_glow_instances.data(),
			                                  static_cast<int>(boss_split_glow_instances.size()));
		}
	}

	void DrawCthulhuEyeLaser()
	{
		const CthulhuEyeLaser& laser = g_EnemySystem.CthulhuLaser;
		if (laser.Phase == CthulhuEyeLaserPhase::Inactive)
		{
			return;
		}
		const float phase_duration = std::max(laser.PhaseDuration, 0.0001f);
		const float progress = 1.0f - Saturate(laser.Timer / phase_duration);
		const float rotation = std::atan2(laser.End.y - laser.Start.y, laser.End.x - laser.Start.x);
		if (laser.Phase == CthulhuEyeLaserPhase::Telegraph &&
		    g_EnemySystem.BossJellyTelegraphTextureID != TEXTURE_INVALID_ID)
		{
			const float pulse = 0.72f + 0.28f * std::abs(std::sin(progress * DirectX::XM_PI * 5.0f));
			static std::vector<SpriteInstance> telegraph_instances;
			telegraph_instances.clear();
			AppendBossTelegraphLine(telegraph_instances, laser.Start, laser.End, 15.0f + progress * 11.0f,
			                        { 0.32f, 1.0f, 0.82f, (0.24f + progress * 0.30f) * pulse });
			SpriteInstanced_DrawUnlit(g_EnemySystem.BossJellyTelegraphTextureID, telegraph_instances.data(),
			                          static_cast<int>(telegraph_instances.size()));
		}
		if (laser.Phase == CthulhuEyeLaserPhase::Firing && g_EnemySystem.CthulhuEyeLaserTextureID != TEXTURE_INVALID_ID)
		{
			const float beam_length = Distance(laser.Start, laser.End);
			const SpriteInstance beam_instance{
				{ (laser.Start.x + laser.End.x) * 0.5f, (laser.Start.y + laser.End.y) * 0.5f },
				{ beam_length, 240.0f },
				rotation,
				{ 1.0f, 1.0f, 1.0f, 0.98f },
				{ 0.0f, 0.0f },
				{ 1.0f, 1.0f },
				0.0f,
			};
			SpriteInstanced_DrawUnlit(g_EnemySystem.CthulhuEyeLaserTextureID, &beam_instance, 1);
		}
		if (g_EnemySystem.CthulhuEyeChargeTextureID != TEXTURE_INVALID_ID)
		{
			const float pulse = laser.Phase == CthulhuEyeLaserPhase::Telegraph
			                        ? 1.0f + 0.06f * std::sin(progress * DirectX::XM_PI * 6.0f)
			                        : 1.08f;
			const float alpha = laser.Phase == CthulhuEyeLaserPhase::Telegraph ? 0.72f + progress * 0.24f : 1.0f;
			const SpriteInstance eye_instance{
				laser.Start,    { 165.0f * pulse, 110.0f * pulse },
				0.0f,           { 1.0f, 1.0f, 1.0f, alpha },
				{ 0.0f, 0.0f }, { 1.0f, 1.0f },
				0.0f,
			};
			SpriteInstanced_DrawUnlit(g_EnemySystem.CthulhuEyeChargeTextureID, &eye_instance, 1);
		}
	}

	void DrawProjectiles()
	{
		// 보스 탄막을 먼저 그리고 일반 몬스터 투사체를 위에 얹는다.
		if (g_EnemySystem.BossJellyTextureID != TEXTURE_INVALID_ID)
		{
			static std::vector<SpriteInstance> jelly_instances;
			static std::vector<SpriteInstance> jelly_glow_instances;
			jelly_instances.clear();
			jelly_glow_instances.clear();
			if (jelly_instances.capacity() < EnemyConstants::BossJelly::BulletMax)
			{
				jelly_instances.reserve(EnemyConstants::BossJelly::BulletMax);
				jelly_glow_instances.reserve(EnemyConstants::BossJelly::BulletMax);
			}
			for (const BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
			{
				if (!bullet.IsActive || bullet.Style != BossProjectileStyle::Jelly)
				{
					continue;
				}
				jelly_glow_instances.push_back({
				    bullet.Position,
				    { bullet.Size * 1.42f, bullet.Size * 1.42f },
				    bullet.Rotation,
				    bullet.IsPrimary ? DirectX::XMFLOAT4{ 0.34f, 1.0f, 0.10f, 0.32f }
				                     : DirectX::XMFLOAT4{ 0.62f, 1.0f, 0.16f, 0.25f },
				});
				jelly_instances.push_back({
				    bullet.Position,
				    { bullet.Size, bullet.Size },
				    bullet.Rotation,
				    bullet.IsPrimary ? DirectX::XMFLOAT4{ 0.82f, 1.0f, 0.72f, 0.96f }
				                     : DirectX::XMFLOAT4{ 0.94f, 1.0f, 0.58f, 0.90f },
				    { 0.0f, 0.0f },
				    { 1.0f, 1.0f },
				    0.0f,
				});
			}
			if (!jelly_glow_instances.empty())
			{
				SpriteInstanced_DrawAdditiveUnlit(g_EnemySystem.BossJellyTextureID, jelly_glow_instances.data(),
				                                  static_cast<int>(jelly_glow_instances.size()));
			}
			if (!jelly_instances.empty())
			{
				SpriteInstanced_DrawOutlinedUnlit(g_EnemySystem.BossJellyTextureID, jelly_instances.data(),
				                                  static_cast<int>(jelly_instances.size()),
				                                  { 1.0f, 0.035f, 0.015f, 0.96f }, 1.0f);
			}
		}
		if (g_EnemySystem.CorruptionProjectileTextureID != TEXTURE_INVALID_ID)
		{
			static std::vector<SpriteInstance> corruption_projectile_instances;
			static std::vector<SpriteInstance> corruption_projectile_glow_instances;
			corruption_projectile_instances.clear();
			corruption_projectile_glow_instances.clear();
			if (corruption_projectile_instances.capacity() < EnemyConstants::BossJelly::BulletMax)
			{
				corruption_projectile_instances.reserve(EnemyConstants::BossJelly::BulletMax);
				corruption_projectile_glow_instances.reserve(EnemyConstants::BossJelly::BulletMax);
			}
			for (const BossJellyBullet& bullet : g_EnemySystem.BossJellyBullets)
			{
				if (!bullet.IsActive || bullet.Style == BossProjectileStyle::Jelly)
				{
					continue;
				}
				const float pulse =
				    0.94f + 0.08f * std::sin(g_EnemySystem.MonsterAnimationElapsed * 12.0f + bullet.Rotation * 2.0f);
				const float visual_size = bullet.Size * bullet.VisualScale;
				const DirectX::XMFLOAT4 glow_color = GetBossProjectileGlowColor(bullet.Style);
				const DirectX::XMFLOAT4 core_color = GetBossProjectileCoreColor(bullet.Style);
				corruption_projectile_glow_instances.push_back({
				    bullet.Position,
				    { visual_size * 1.62f * pulse, visual_size * 1.62f * pulse },
				    bullet.Rotation,
				    glow_color,
				});
				corruption_projectile_instances.push_back({
				    bullet.Position,
				    { visual_size, visual_size },
				    bullet.Rotation,
				    core_color,
				    { 0.0f, 0.0f },
				    { 1.0f, 1.0f },
				    0.0f,
				});
			}
			if (!corruption_projectile_glow_instances.empty())
			{
				SpriteInstanced_DrawAdditiveUnlit(g_EnemySystem.CorruptionProjectileTextureID,
				                                  corruption_projectile_glow_instances.data(),
				                                  static_cast<int>(corruption_projectile_glow_instances.size()));
				SpriteInstanced_DrawOutlinedUnlit(
				    g_EnemySystem.CorruptionProjectileTextureID, corruption_projectile_instances.data(),
				    static_cast<int>(corruption_projectile_instances.size()), { 1.0f, 0.035f, 0.015f, 0.96f }, 1.0f);
			}
		}
		EnemyAttackPattern::DrawProjectiles();
		DrawCthulhuEyeLaser();
	}

	int AppendPointLights(SpritePointLight* lights, int light_count, int capacity,
	                      const DirectX::XMFLOAT2& camera_position, const DirectX::XMFLOAT2& viewport_size)
	{
		light_count = std::clamp(light_count, 0, std::max(capacity, 0));
		if (!lights || capacity <= 0)
		{
			return light_count;
		}

		struct LightCandidate
		{
			float DistanceSquared{ 0.0f };
			DirectX::XMFLOAT2 Position{};
			MonsterType Type{ EnemyConstants::Spawn::DefaultMonsterType };
			int EnemyID{ 0 };
		};

		// 화면에 가까운 발광 몬스터부터 골라 남은 라이트 슬롯을 채운다.
		static std::vector<LightCandidate> candidates;
		candidates.clear();
		if (candidates.capacity() < GameEnemy::ENEMY_CAPACITY)
		{
			candidates.reserve(GameEnemy::ENEMY_CAPACITY);
		}
		const float visible_radius = Length(viewport_size) * 0.58f + 240.0f;
		const float visible_radius_squared = visible_radius * visible_radius;
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			const EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
			if (!IsAliveAndTargetable(enemy_id))
			{
				continue;
			}
			const DirectX::XMFLOAT2 position = slot.Entity.GetPosition();
			const float distance_squared = DistanceSquared(position, camera_position);
			if (distance_squared <= visible_radius_squared)
			{
				candidates.push_back({ distance_squared, position, slot.Type, enemy_id });
			}
		}
		std::sort(candidates.begin(), candidates.end(),
		          [](const LightCandidate& left, const LightCandidate& right)
		          {
			          return left.DistanceSquared < right.DistanceSquared;
		          });
		const int append_count = std::min({ capacity - light_count, static_cast<int>(candidates.size()) });
		const float stage_light_strength_scale = ProceduralMap_GetRound() == 1 ? 0.55f : 1.0f;
		for (int i = 0; i < append_count; ++i)
		{
			const LightCandidate& candidate = candidates[i];
			const MonsterLightStyle style = GetMonsterLightStyle(candidate.Type);
			const float flicker = 0.98f + 0.02f * std::sin(g_EnemySystem.MonsterAnimationElapsed * 8.0f +
			                                               static_cast<float>(candidate.EnemyID) * 1.73f);
			lights[light_count++] = {
				candidate.Position,
				style.Radius,
				style.Strength * stage_light_strength_scale * flicker,
				style.Color,
			};
		}
		static std::vector<int> jelly_light_candidates;
		jelly_light_candidates.clear();
		if (jelly_light_candidates.capacity() < EnemyConstants::BossJelly::BulletMax)
		{
			jelly_light_candidates.reserve(EnemyConstants::BossJelly::BulletMax);
		}
		for (int bullet_id = 0; bullet_id < EnemyConstants::BossJelly::BulletMax; ++bullet_id)
		{
			const BossJellyBullet& bullet = g_EnemySystem.BossJellyBullets[bullet_id];
			if (!bullet.IsActive)
			{
				continue;
			}
			jelly_light_candidates.push_back(bullet_id);
		}
		const int jelly_light_count =
		    std::min({ capacity - light_count, static_cast<int>(jelly_light_candidates.size()) });
		for (int i = 0; i < jelly_light_count; ++i)
		{
			const int bullet_id = jelly_light_candidates[i];
			const BossJellyBullet& bullet = g_EnemySystem.BossJellyBullets[bullet_id];
			const float flicker = 0.94f + 0.06f * std::sin(g_EnemySystem.MonsterAnimationElapsed * 10.0f +
			                                               static_cast<float>(bullet_id) * 1.37f);
			const bool corruption = bullet.Style != BossProjectileStyle::Jelly;
			lights[light_count++] = {
				bullet.Position,
				corruption ? 175.0f : (bullet.IsPrimary ? 220.0f : 145.0f),
				(corruption ? 0.58f : (bullet.IsPrimary ? 0.68f : 0.46f)) * flicker,
				corruption ? GetBossProjectileLightColor(bullet.Style)
				           : (bullet.IsPrimary ? DirectX::XMFLOAT3{ 0.30f, 1.0f, 0.08f }
				                               : DirectX::XMFLOAT3{ 0.58f, 1.0f, 0.12f }),
			};
		}
		return light_count;
	}
} // namespace GameEnemy
