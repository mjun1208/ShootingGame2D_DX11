#include "game_enemy_internal.h"

#include <algorithm>
#include "game_enemy.h"
#include "Constants/enemy_constants.h"

namespace GameEnemy::Internal
{
	// 적 시스템 전체가 공유하는 런타임 상태는 구현 진입점과 함께 둔다.
	EnemySystemState g_EnemySystem{};
} // namespace GameEnemy::Internal

namespace GameEnemy
{
	using namespace Internal;

	// 적 시스템의 시작, 리셋, 프레임 갱신 진입점.
	void Initialize()
	{
		// 효과음과 공용 공격 이미지는 적 시스템이 한 번만 들고 있는다.
		for (int& audio_id : g_EnemySystem.DashSlashHitAudioIDs)
		{
			audio_id = Audio_Load("asset/sound/leohpaz-22-slash-04.wav");
		}
		g_EnemySystem.SlimeDamageAudioID = Audio_Load("asset/sound/slime-02.wav");
		g_EnemySystem.SlimeDeathAudioID = Audio_Load("asset/sound/slime-05.wav");
		g_EnemySystem.BossSlimeProjectileFireAudioID = Audio_Load("asset/sound/slime-07.wav");
		g_EnemySystem.BatDamageAudioID = Audio_Load("asset/sound/freesound-468442-bat-damage.wav");
		g_EnemySystem.BatDeathAudioID = Audio_Load("asset/sound/freesound-445958-bat-death.wav");
		g_EnemySystem.BatDashAudioID = Audio_Load("asset/sound/pixabay-fast-swoosh-03-229316.wav");
		g_EnemySystem.BonesDamageAudioID = Audio_Load("asset/sound/bones-rattle-2-damage.wav");
		g_EnemySystem.BonesDeathAudioID = Audio_Load("asset/sound/bones-rattle-0-death.wav");
		for (int& audio_id : g_EnemySystem.SkeletonMageFireAudioIDs)
		{
			audio_id = Audio_Load("asset/sound/mixkit-wizard-fire-woosh-1326.wav");
		}
		g_EnemySystem.EnemyWarriorSlashAudioID = Audio_Load("asset/sound/leohpaz-22-slash-04.wav");
		for (std::size_t i = 0; i < EnemyConstants::Audio::DamageSoundPaths.size(); ++i)
		{
			g_EnemySystem.OrcDamageAudioIDs[i] = Audio_Load(EnemyConstants::Audio::DamageSoundPaths[i]);
		}
		g_EnemySystem.OrcDeathAudioID = Audio_Load("asset/sound/leohpaz-24-orc-death-spin.wav");
		g_EnemySystem.OrcShamanCastAudioID = Audio_Load("asset/sound/mixkit-wizard-fire-woosh-1326.wav");
		g_EnemySystem.CthulhuLaserAudioID = Audio_Load("asset/sound/pixabay-plasma-ku-05-233818.wav");

		// 몬스터별 이미지는 monsters.json에 적힌 경로를 사용한다.
		for (MonsterType type : MONSTER_TYPES)
		{
			const std::size_t type_index = MonsterTypeToIndex(type);
			g_EnemySystem.MonsterTextureIDs[type_index] = Texture_Load(GetMonsterData(type).TexturePath.c_str(), false);
			g_EnemySystem.MonsterTextureSizes[type_index] =
			    Texture_GetSize(g_EnemySystem.MonsterTextureIDs[type_index]);
		}
		// 가장 큰 몬스터가 한 셀 안에서 검색되도록 공간 해시 크기를 맞춘다.
		g_EnemySystem.EnemyCollisionCellSize = 1.0f;
		for (MonsterType type : MONSTER_TYPES)
		{
			g_EnemySystem.EnemyCollisionCellSize =
			    std::max(g_EnemySystem.EnemyCollisionCellSize, GetMonsterData(type).CollisionRadius * 2.0f);
		}
		g_EnemySystem.BoneDropTextureID =
		    Texture_Load(L"asset/texture/character/monster/04_skeleton/04_skeleton_white_bones.png", false);
		g_EnemySystem.DaggerTextureID = Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_icon-sword.png", false);
		g_EnemySystem.AxeTextureID = Texture_Load(L"asset/texture/ui/dark_rpg_gui/dfgui_icon-axe.png", false);
		g_EnemySystem.MageProjectileTextureID = Texture_Load(L"asset/texture/vfx/umplix/fireball/fireball.png", false);
		g_EnemySystem.SpawnTelegraphTextureID =
		    Texture_Load(L"asset/texture/vfx/spawn_magic_circle/magic_circle.png", false);
		g_EnemySystem.BossJellyTextureID = Texture_Load(L"asset/texture/projectile/venom_comet.png", false);
		g_EnemySystem.BossJellyTelegraphTextureID = Texture_Load(L"asset/texture/white_square.png", false);
		g_EnemySystem.CorruptionProjectileTextureID = Texture_Load(L"asset/texture/projectile/void_orb.png", false);
		g_EnemySystem.CorruptionAuraTextureID =
		    Texture_Load(L"asset/texture/vfx/corruption_flame/corruption_flame_4x2.png", false);
		g_EnemySystem.CthulhuEyeChargeTextureID =
		    Texture_Load(L"asset/texture/vfx/cthulhu/cthulhu_eye_charge.png", false);
		g_EnemySystem.CthulhuEyeLaserTextureID =
		    Texture_Load(L"asset/texture/vfx/cthulhu/cthulhu_eye_laser.png", false);

		// 패턴 쪽은 리소스를 직접 소유하지 않고 여기서 만든 ID를 빌려 쓴다.
		EnemyAttackPattern::Resources pattern_resources{};
		pattern_resources.BoneTextureID = g_EnemySystem.BoneDropTextureID;
		pattern_resources.DaggerTextureID = g_EnemySystem.DaggerTextureID;
		pattern_resources.AxeTextureID = g_EnemySystem.AxeTextureID;
		pattern_resources.MageProjectileTextureID = g_EnemySystem.MageProjectileTextureID;
		pattern_resources.WarningTextureID = g_EnemySystem.BossJellyTelegraphTextureID;
		pattern_resources.GroundEffectTextureID = g_EnemySystem.SpawnTelegraphTextureID;
		pattern_resources.BoneThrowAudioID = g_EnemySystem.BonesDamageAudioID;
		pattern_resources.MageFireAudioIDs = g_EnemySystem.SkeletonMageFireAudioIDs;
		pattern_resources.WarriorSlashAudioID = g_EnemySystem.EnemyWarriorSlashAudioID;
		pattern_resources.BatDashAudioID = g_EnemySystem.BatDashAudioID;
		pattern_resources.ShamanCastAudioID = g_EnemySystem.OrcShamanCastAudioID;
		EnemyAttackPattern::Initialize(pattern_resources);
		ResetDungeon();
	}

	void Finalize()
	{
		ResetDungeon();
		// 패턴을 먼저 비운 뒤 패턴에서 쓰던 리소스를 해제한다.
		EnemyAttackPattern::Finalize();
		g_EnemySystem.RoomWaves.clear();
		for (int& audio_id : g_EnemySystem.DashSlashHitAudioIDs)
		{
			if (audio_id >= 0)
			{
				Audio_Unload(audio_id);
				audio_id = -1;
			}
		}
		if (g_EnemySystem.SlimeDamageAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.SlimeDamageAudioID);
			g_EnemySystem.SlimeDamageAudioID = -1;
		}
		if (g_EnemySystem.SlimeDeathAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.SlimeDeathAudioID);
			g_EnemySystem.SlimeDeathAudioID = -1;
		}
		if (g_EnemySystem.BossSlimeProjectileFireAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BossSlimeProjectileFireAudioID);
			g_EnemySystem.BossSlimeProjectileFireAudioID = -1;
		}
		if (g_EnemySystem.BatDamageAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BatDamageAudioID);
			g_EnemySystem.BatDamageAudioID = -1;
		}
		if (g_EnemySystem.BatDeathAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BatDeathAudioID);
			g_EnemySystem.BatDeathAudioID = -1;
		}
		if (g_EnemySystem.BatDashAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BatDashAudioID);
			g_EnemySystem.BatDashAudioID = -1;
		}
		if (g_EnemySystem.BonesDamageAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BonesDamageAudioID);
			g_EnemySystem.BonesDamageAudioID = -1;
		}
		if (g_EnemySystem.BonesDeathAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.BonesDeathAudioID);
			g_EnemySystem.BonesDeathAudioID = -1;
		}
		for (int& audio_id : g_EnemySystem.SkeletonMageFireAudioIDs)
		{
			if (audio_id >= 0)
			{
				Audio_Unload(audio_id);
				audio_id = -1;
			}
		}
		if (g_EnemySystem.EnemyWarriorSlashAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.EnemyWarriorSlashAudioID);
			g_EnemySystem.EnemyWarriorSlashAudioID = -1;
		}
		for (int& audio_id : g_EnemySystem.OrcDamageAudioIDs)
		{
			if (audio_id >= 0)
			{
				Audio_Unload(audio_id);
				audio_id = -1;
			}
		}
		if (g_EnemySystem.OrcDeathAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.OrcDeathAudioID);
			g_EnemySystem.OrcDeathAudioID = -1;
		}
		if (g_EnemySystem.OrcShamanCastAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.OrcShamanCastAudioID);
			g_EnemySystem.OrcShamanCastAudioID = -1;
		}
		if (g_EnemySystem.CthulhuLaserAudioID >= 0)
		{
			Audio_Unload(g_EnemySystem.CthulhuLaserAudioID);
			g_EnemySystem.CthulhuLaserAudioID = -1;
		}
		for (int& texture_id : g_EnemySystem.MonsterTextureIDs)
		{
			Texture_Release(texture_id);
			texture_id = TEXTURE_INVALID_ID;
		}
		Texture_Release(g_EnemySystem.BoneDropTextureID);
		g_EnemySystem.BoneDropTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.DaggerTextureID);
		g_EnemySystem.DaggerTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.AxeTextureID);
		g_EnemySystem.AxeTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.MageProjectileTextureID);
		g_EnemySystem.MageProjectileTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.SpawnTelegraphTextureID);
		g_EnemySystem.SpawnTelegraphTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.BossJellyTextureID);
		g_EnemySystem.BossJellyTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.BossJellyTelegraphTextureID);
		g_EnemySystem.BossJellyTelegraphTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.CorruptionProjectileTextureID);
		g_EnemySystem.CorruptionProjectileTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.CorruptionAuraTextureID);
		g_EnemySystem.CorruptionAuraTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.CthulhuEyeChargeTextureID);
		g_EnemySystem.CthulhuEyeChargeTextureID = TEXTURE_INVALID_ID;
		Texture_Release(g_EnemySystem.CthulhuEyeLaserTextureID);
		g_EnemySystem.CthulhuEyeLaserTextureID = TEXTURE_INVALID_ID;
	}

	void ResetDungeon()
	{
		// 이전 라운드에서 남은 투사체, 예약 스폰, 화면 연출을 먼저 비운다.
		ProceduralMap_ClearEncounterLock();
		g_EnemySystem.RoundCleared = false;
		g_EnemySystem.MonsterAnimationElapsed = 0.0f;
		g_EnemySystem.PendingEnemySpawns.clear();
		g_EnemySystem.SpawnArrivalEffects.clear();
		g_EnemySystem.BoneDropEffects.clear();
		g_EnemySystem.CutDustParticles.clear();
		g_EnemySystem.PendingDashSlashAttacks.clear();
		g_EnemySystem.PendingBossDefeat = {};
		g_EnemySystem.BossDefeatedEventPending = false;
		g_EnemySystem.CombatFeedback = {};
		ClearBossJellyBullets();
		g_EnemySystem.BossJellyFireCooldown = EnemyConstants::BossJelly::FireInterval;
		g_EnemySystem.BossJellyTelegraphEnemyID = -1;
		g_EnemySystem.BossJellyTelegraphDirection = { 1.0f, 0.0f };
		g_EnemySystem.BossSplitStage = 0;
		g_EnemySystem.BossVolleySequence = 0;
		g_EnemySystem.CorruptionFireCooldown = 0.8f;
		g_EnemySystem.CorruptionVolleySequence = 0;
		g_EnemySystem.CthulhuActivePattern = -1;
		g_EnemySystem.CthulhuPatternStep = 0;
		g_EnemySystem.CthulhuLaser = {};
		g_EnemySystem.CthulhuDashBulletCooldown = 0.0f;
		g_EnemySystem.CthulhuDashBulletSequence = 0;
		// 적 배열은 다시 할당하지 않고 슬롯 내용만 초기 상태로 되돌린다.
		for (EnemySlot& slot : g_EnemySystem.EnemySlots)
		{
			slot.Entity = cEnemy{};
			slot.Type = EnemyConstants::Spawn::DefaultMonsterType;
			slot.RoomIndex = -1;
			slot.DrawScale = 1.0f;
			slot.ExperienceScale = 1.0f;
			slot.BossFireScaleElapsed = EnemyConstants::BossJelly::FireScaleDuration;
			slot.BossSplitScaleElapsed = EnemyConstants::BossBody::SplitScaleDuration;
		}
		EnemyAttackPattern::Reset();

		// 새 맵의 방 개수에 맞춰 탐험/웨이브 진행 상태를 다시 만든다.
		g_EnemySystem.RoomWaves.assign(ProceduralMap_GetRoomCount(), RoomWaveRuntime{});
		g_EnemySystem.DiscoveredRooms.assign(ProceduralMap_GetRoomCount(), false);
		const int start_room = ProceduralMap_GetStartRoomIndex();
		if (start_room >= 0 && start_room < static_cast<int>(g_EnemySystem.DiscoveredRooms.size()))
		{
			g_EnemySystem.DiscoveredRooms[start_room] = true;
		}
		if (start_room >= 0 && start_room < static_cast<int>(g_EnemySystem.RoomWaves.size()) &&
		    start_room != ProceduralMap_GetFinalEncounterRoomIndex())
		{
			g_EnemySystem.RoomWaves[start_room].State = RoomWaveState::Cleared;
		}
		for (int room_index = 0; room_index < static_cast<int>(g_EnemySystem.RoomWaves.size()); ++room_index)
		{
			const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
			if (room && room->IsPortalRoom)
			{
				g_EnemySystem.RoomWaves[room_index].State = RoomWaveState::Cleared;
			}
		}
		g_EnemySystem.CurrentRoomIndex = start_room;
		BuildEnemyCollisionGrid();
	}

	void Update(float delta_time)
	{
		delta_time = std::max(delta_time, 0.0f);
		const float safe_delta_time = delta_time;

		// 공격 판정과 별개인 등장/사망 파티클부터 시간만 진행한다.
		g_EnemySystem.MonsterAnimationElapsed =
		    std::fmod(g_EnemySystem.MonsterAnimationElapsed + safe_delta_time, 60.0f);
		for (PendingEnemySpawn& pending : g_EnemySystem.PendingEnemySpawns)
		{
			pending.Elapsed += safe_delta_time;
		}
		for (SpawnArrivalEffect& arrival : g_EnemySystem.SpawnArrivalEffects)
		{
			arrival.Elapsed += safe_delta_time;
		}
		std::erase_if(g_EnemySystem.SpawnArrivalEffects,
		              [](const SpawnArrivalEffect& arrival)
		              {
			              return arrival.Elapsed >= EnemyConstants::Spawn::ArrivalRingDuration;
		              });
		for (BoneDropEffect& bone_drop : g_EnemySystem.BoneDropEffects)
		{
			bone_drop.Elapsed += safe_delta_time;
			bone_drop.Velocity.y += BONE_BURST_GRAVITY * safe_delta_time;
			bone_drop.Position.x += bone_drop.Velocity.x * safe_delta_time;
			bone_drop.Position.y += bone_drop.Velocity.y * safe_delta_time;
			const float drag = std::pow(BONE_BURST_DRAG_PER_SECOND, safe_delta_time);
			bone_drop.Velocity.x *= drag;
			bone_drop.Velocity.y *= drag;
			bone_drop.Rotation += bone_drop.AngularVelocity * safe_delta_time;
		}
		std::erase_if(g_EnemySystem.BoneDropEffects,
		              [](const BoneDropEffect& bone_drop)
		              {
			              return bone_drop.Elapsed >= bone_drop.Lifetime;
		              });
		for (CutDustParticle& particle : g_EnemySystem.CutDustParticles)
		{
			particle.Elapsed += safe_delta_time;
			particle.Velocity.y += CUT_DUST_GRAVITY * safe_delta_time;
			particle.Position.x += particle.Velocity.x * safe_delta_time;
			particle.Position.y += particle.Velocity.y * safe_delta_time;
			const float drag = std::pow(CUT_DUST_DRAG_PER_SECOND, safe_delta_time);
			particle.Velocity.x *= drag;
			particle.Velocity.y *= drag;
			particle.Rotation += particle.AngularVelocity * safe_delta_time;
		}
		std::erase_if(g_EnemySystem.CutDustParticles,
		              [](const CutDustParticle& particle)
		              {
			              return particle.Elapsed >= particle.Lifetime;
		              });

		// 플레이어가 방을 옮겼는지 확인하고 필요한 웨이브를 시작한다.
		const DirectX::XMFLOAT2 player_position = GamePlayer::GetPosition();
		const int room_index = ProceduralMap_GetRoomIndexAt(player_position);
		if (room_index >= 0 && room_index < static_cast<int>(g_EnemySystem.DiscoveredRooms.size()))
		{
			g_EnemySystem.DiscoveredRooms[room_index] = true;
		}
		if (room_index >= 0 && room_index != g_EnemySystem.CurrentRoomIndex)
		{
			AbandonOtherActiveRooms(room_index);
			g_EnemySystem.CurrentRoomIndex = room_index;
		}
		if (room_index >= 0)
		{
			BeginRoomEncounter(room_index, player_position);
		}

		// 개별 이동과 공격 선택은 EnemyAttackPattern에 맡긴다.
		for (int enemy_id = 0; enemy_id < GameEnemy::ENEMY_CAPACITY; ++enemy_id)
		{
			EnemySlot& slot = g_EnemySystem.EnemySlots[enemy_id];
			const MonsterData& monster_data = GetMonsterData(slot.Type);
			const float visual_bottom_offset = monster_data.DrawSize.y * slot.DrawScale * 0.5f;
			slot.BossFireScaleElapsed =
			    std::min(slot.BossFireScaleElapsed + safe_delta_time, EnemyConstants::BossJelly::FireScaleDuration);
			slot.BossSplitScaleElapsed =
			    std::min(slot.BossSplitScaleElapsed + safe_delta_time, EnemyConstants::BossBody::SplitScaleDuration);
			EnemyAttackPattern::UpdateEnemy(enemy_id, slot.Type, slot.Entity, delta_time, player_position,
			                                visual_bottom_offset, g_EnemySystem.MonsterAnimationElapsed);
			if (!slot.Entity.IsActive())
			{
				slot.RoomIndex = -1;
			}
		}
		// 보스 패턴과 적 투사체는 일반 몬스터 이동이 끝난 위치를 기준으로 돈다.
		TrySplitBossBody(player_position);
		UpdateBossJellyPattern(safe_delta_time, player_position);
		UpdateCorruptedBossPattern(safe_delta_time, player_position);
		EnemyAttackPattern::UpdateProjectiles(safe_delta_time);
		ResolveEnemyOverlaps();

		UpdateRoomEncounter(g_EnemySystem.CurrentRoomIndex, delta_time, player_position);
		// 웨이브 갱신 중 새 적이 생길 수 있으므로 공간 해시는 마지막에 다시 만든다.
		BuildEnemyCollisionGrid();
	}
} // namespace GameEnemy
