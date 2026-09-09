#include "ingame_scene.h"
#include "Constants/scene_constants.h"

#include "Audio.h"
#include "bgm.h"
#include "blood.h"
#include "collision.h"
#include "config.h"
#include "game_bullet.h"
#include "game_damage_text.h"
#include "game_data_manager.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "game_experience_gem.h"
#include "game_healing_item.h"
#include "game_player.h"
#include "input_keyboard.h"
#include "input_mouse.h"
#include "procedural_map.h"
#include "scene_manager.h"
#include "slime_goo.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"
#include "time_stop_effect.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{

	BgmTrack GetRoundBgmTrack(int round)
	{
		if (round <= 1)
		{
			return BgmTrack::Forest;
		}
		if (round == 2)
		{
			return BgmTrack::Dungeon1;
		}
		return BgmTrack::Dungeon2;
	}

	namespace WorldLighting
	{
		struct Profile
		{
			float AmbientBrightness;
			DirectX::XMFLOAT3 DirectLightColor;
			float DirectLightStrength;
			float PlayerLightRadius;
			float PlayerLightStrength;
			DirectX::XMFLOAT3 PlayerLightColor;
		};

		Profile GetActiveProfile()
		{
			if (ProceduralMap_GetRound() == 1)
			{
				return {
					0.86f, { 1.0f, 0.96f, 0.78f }, 0.16f, 270.0f, 0.18f, { 1.0f, 0.92f, 0.70f },
				};
			}

			return {
				0.52f, { 0.78f, 0.86f, 1.0f }, 0.14f, 330.0f, 0.88f, { 1.0f, 0.70f, 0.38f },
			};
		}
	} // namespace WorldLighting

	void ApplyWorldLighting(const cChainLightning& chain_lightning,
	                        const std::vector<std::unique_ptr<Chest>>& reward_chests, bool portal_open,
	                        const DirectX::XMFLOAT2& camera_position, const DirectX::XMFLOAT2& viewport_size)
	{
		const WorldLighting::Profile profile = WorldLighting::GetActiveProfile();
		std::array<SpritePointLight, SPRITE_POINT_LIGHT_CAPACITY> lights{};
		int light_count = 0;
		lights[light_count++] = {
			GamePlayer::GetPosition(),
			profile.PlayerLightRadius,
			profile.PlayerLightStrength,
			profile.PlayerLightColor,
		};
		for (const std::unique_ptr<Chest>& chest : reward_chests)
		{
			if (!chest || light_count >= SPRITE_POINT_LIGHT_CAPACITY)
			{
				break;
			}
			SpritePointLight chest_light{};
			if (chest->BuildPointLight(chest_light))
			{
				lights[light_count++] = chest_light;
			}
		}
		if (portal_open && light_count < SPRITE_POINT_LIGHT_CAPACITY)
		{
			lights[light_count++] = {
				ProceduralMap_GetRoundExitPosition(),
				300.0f,
				0.82f,
				{ 0.48f, 0.22f, 1.0f },
			};
		}
		light_count = chain_lightning.AppendPointLights(lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY);
		light_count = GameEnemy::AppendPointLights(lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY,
		                                           camera_position, viewport_size);
		light_count = cGameEffectManager::GetInstance().AppendPointLights(lights.data(), light_count,
		                                                                  SPRITE_POINT_LIGHT_CAPACITY);
		light_count = ProceduralMap_AppendTorchLights(lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY,
		                                              camera_position, viewport_size);

		SpriteLighting_SetWorldLighting(profile.AmbientBrightness, profile.DirectLightColor,
		                                profile.DirectLightStrength, lights.data(), light_count);
	}

	DirectX::XMFLOAT2 GetGameplayCameraTarget()
	{
		const DirectX::XMFLOAT2 player_position = GamePlayer::GetPosition();
		const int room_index = ProceduralMap_GetRoomIndexAt(player_position);
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room)
		{
			return player_position;
		}

		return {
			player_position.x + (room->Center.x - player_position.x) * SceneConstants::CameraFollow::RoomCenterWeight,
			player_position.y + (room->Center.y - player_position.y) * SceneConstants::CameraFollow::RoomCenterWeight
		};
	}

	DirectX::XMFLOAT2 SmoothCameraFollow(const DirectX::XMFLOAT2& current, const DirectX::XMFLOAT2& target,
	                                     float delta_time)
	{
		const float follow_ratio =
		    1.0f - std::exp(-SceneConstants::CameraFollow::SmoothSpeed * std::max(delta_time, 0.0f));
		return { current.x + (target.x - current.x) * follow_ratio, current.y + (target.y - current.y) * follow_ratio };
	}

} // namespace

IngameScene::~IngameScene() = default;

bool IngameScene::TryStartBossDeathPresentation()
{
	if (!m_CombatPresentation.TryBeginBossDeath(m_Camera, m_ChainLightning))
	{
		return false;
	}

	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	return true;
}

bool IngameScene::Initialize()
{
	// 맵을 그릴 기반과 필수 이미지를 먼저 준비한다.
	if (!SpriteInstanced_Initialize())
	{
		return false;
	}
	if (!ProceduralMap_Initialize())
	{
		SpriteInstanced_Finalize();
		return false;
	}
	m_DeathOverlayTextureID = Texture_Load(L"asset/texture/map/dungeon/structure/void_deep.png", false);
	if (m_DeathOverlayTextureID == TEXTURE_INVALID_ID)
	{
		Texture_Release(m_DeathOverlayTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	if (!Blood::Initialize())
	{
		Texture_Release(m_DeathOverlayTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	if (!cSlimeGoo::GetInstance().Initialize())
	{
		Blood::Finalize();
		Texture_Release(m_DeathOverlayTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	// 플레이어 위치를 잡은 뒤 카메라와 전투 시스템을 붙인다.
	m_Camera.SetScreenSize((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
	m_Camera.SetZoom(1.0f);

	GamePlayer::Initialize();
	GameExperienceGem::Initialize();
	GameHealingItem::Initialize();
	GamePlayer::SetPosition(ProceduralMap_GetPlayerSpawnPosition());
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(GetGameplayCameraTarget(), m_Camera.GetScreenSize()));
	GameBullet::Initialize();
	GameEnemy::Initialize();
	cGameEffectManager::GetInstance().Initialize();
	m_RewardedRooms.assign(ProceduralMap_GetRoomCount(), false);
	std::unique_ptr<Chest> initial_weapon_chest = std::make_unique<Chest>();
	initial_weapon_chest->Initialize(ProceduralMap_GetPlayerSpawnPosition());
	m_InitialWeaponChest = initial_weapon_chest.get();
	m_RewardChests.push_back(std::move(initial_weapon_chest));
	m_RewardChestRoomIndices.push_back(ProceduralMap_GetStartRoomIndex());
	UpdateRewardChestRoomLock();
	m_RoundPortal.Initialize(ProceduralMap_GetRoundExitPosition());
	GameDamageText::Initialize();
	m_ChainLightning.Initialize();
	m_TimeStopActivateAudioID = Audio_Load("asset/sound/leohpaz-88-teleport-02.wav");
	// HUD나 메뉴 초기화가 실패하면 위에서 만든 것까지 Finalize에서 정리한다.
	CollisionSystem_Initialize();
	if (!m_CombatPresentation.Initialize() || !m_Hud.Initialize(m_Camera.GetScreenSize()) ||
	    !m_MenuController.Initialize() || !m_BossIntro.Initialize())
	{
		Finalize();
		return false;
	}
	// 씬을 다시 들어와도 이전 플레이의 연출 상태가 남지 않게 한다.
	m_CurrentRound = 1;
	m_FadeAlpha = 1.0f;
	m_TransitionState = RoundTransitionState::FadingIn;
	m_CombatPresentation.Reset();
	m_RoundElapsedTime = 0.0f;
	m_RunElapsedTime = 0.0f;
	m_QSkillCooldownRemaining = 0.0f;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	m_LastAugmentLevel = GamePlayer::GetLevel();
	m_MenuController.Reset();
	TimeStopEffect_Cancel();
	InputMouse_SetVisible(false);

	return true;
}

void IngameScene::Finalize()
{
	// Initialize와 반대 순서로 소리, UI, 게임 오브젝트, 렌더 자원을 정리한다.
	TimeStopEffect_Cancel();
	if (m_TimeStopActivateAudioID >= 0)
	{
		Audio_Unload(m_TimeStopActivateAudioID);
		m_TimeStopActivateAudioID = -1;
	}
	InputMouse_SetVisible(true);
	m_BossIntro.Finalize();
	m_CombatPresentation.Finalize();
	m_MenuController.Finalize();
	m_Hud.Finalize();
	CollisionSystem_Finalize();
	m_ChainLightning.Finalize();
	GameDamageText::Finalize();
	cGameEffectManager::GetInstance().Finalize();
	m_RoundPortal.Finalize();
	ClearRewardChests();
	m_RewardedRooms.clear();
	GameEnemy::Finalize();
	GameBullet::Finalize();
	GameHealingItem::Finalize();
	GameExperienceGem::Finalize();
	GamePlayer::Finalize();
	cSlimeGoo::GetInstance().Finalize();
	Blood::Finalize();
	ProceduralMap_Finalize();
	Texture_Release(m_DeathOverlayTextureID);
	m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
	SpriteInstanced_Finalize();
}

void IngameScene::Update(float delta_time)
{
	// R은 RETRY와 동일하게 씬을 새로 만들어 플레이어와 진행 상태 전체를 초기화한다.
	// 팝업, 시간 정지, 라운드 전환 중에도 재시작 입력을 먼저 처리한다.
	if (InputKeyboard_IsTrigger(KK_R))
	{
		SceneManager_ChangeScene(SceneID::Ingame);
		return;
	}

	delta_time = std::max(delta_time, 0.0f);
	const bool impact_hit_stop_active = m_CombatPresentation.UpdateHitStop(delta_time);
	m_HasAutoAimTarget = false;

	// 팝업이나 연출이 화면을 잡고 있으면 일반 게임 갱신은 여기서 멈춘다.
	if (m_Hud.IsWeaponUnlockPopupOpen())
	{
		m_Hud.UpdateWeaponUnlockPopup();
		return;
	}
	if (impact_hit_stop_active && !m_CombatPresentation.IsBossDeathActive() &&
	    !m_CombatPresentation.IsPlayerDeathActive())
	{
		m_Camera.Update(delta_time);
		return;
	}
	ProceduralMap_Update(delta_time);
	if (m_CombatPresentation.IsBossDeathActive())
	{
		m_Camera.Update(delta_time);
		m_CombatPresentation.UpdateBossDeath(delta_time, m_Camera);
		cGameEffectManager::GetInstance().Update(delta_time);
		return;
	}
	if (m_BossIntro.IsActive())
	{
		m_Camera.Update(delta_time);
		m_BossIntro.Update(delta_time);
		return;
	}
	if (m_MenuController.IsAugmentOpen())
	{
		ApplyAugment(m_MenuController.UpdateAugment());
		return;
	}
	if (!m_CombatPresentation.IsPlayerDeathActive() && GamePlayer::GetLevel() > m_LastAugmentLevel)
	{
		if (!m_MenuController.OpenAugment())
		{
			m_LastAugmentLevel = GamePlayer::GetLevel();
		}
		return;
	}
	if (m_MenuController.IsPauseOpen())
	{
		HandlePauseAction(m_MenuController.UpdatePause());
		return;
	}
	if (!m_CombatPresentation.IsPlayerDeathActive() && InputKeyboard_IsTrigger(KK_ESCAPE))
	{
		m_HasAutoAimTarget = false;
		m_MenuController.OpenPause();
		return;
	}
	// 여기부터는 플레이 가능한 상태다. 카메라와 바닥 연출은 항상 움직인다.
	m_Camera.Update(delta_time);
	Blood::Update(delta_time);
	cSlimeGoo::GetInstance().Update(delta_time);
	if (m_CombatPresentation.IsPlayerDeathActive())
	{
		if (m_CombatPresentation.UpdatePlayerDeath(delta_time, m_Camera))
		{
			SceneManager_ChangeScene(SceneID::GameOver);
		}
		return;
	}
#ifdef _DEBUG
	if (InputKeyboard_IsTrigger(KK_P) && m_TransitionState == RoundTransitionState::None)
	{
		TimeStopEffect_Cancel();
		m_ShowWorldMap = false;
		AdvanceToNextRound();
		return;
	}
#endif
	// Q 스킬은 시간을 멈추고 다음 대시를 강화한다.
	m_QSkillCooldownRemaining = std::max(0.0f, m_QSkillCooldownRemaining - delta_time);
	TimeStopEffect_Update(delta_time);
	if (InputKeyboard_IsTrigger(KK_Q) && !TimeStopEffect_IsActive() && m_QSkillCooldownRemaining <= 0.0f &&
	    m_TransitionState == RoundTransitionState::None && !m_ShowWorldMap)
	{
		m_QSkillCooldownRemaining = IngameHud::GetQSkillCooldownDuration();
		TimeStopEffect_Trigger(m_Camera.WorldToScreen(GamePlayer::GetPosition()));
		if (m_TimeStopActivateAudioID >= 0)
		{
			Audio_Play(m_TimeStopActivateAudioID);
		}
		GamePlayer::BeginEmpoweredDashMode();
		m_Camera.Shake(5.0f, 0.22f);
	}
	const bool time_stopped = TimeStopEffect_IsActive();
	if (!time_stopped)
	{
		GamePlayer::EndEmpoweredDashMode();
	}

	// 라운드 전환과 전체 지도는 전투 입력보다 먼저 처리한다.
	if (!time_stopped && m_TransitionState != RoundTransitionState::None)
	{
		UpdateRoundTransition(delta_time);
		return;
	}

	if (!time_stopped && InputKeyboard_IsTrigger(KK_M))
	{
		m_ShowWorldMap = !m_ShowWorldMap;
	}

	if (m_ShowWorldMap)
	{
		return;
	}

	if (!time_stopped)
	{
		m_RoundElapsedTime += delta_time;
		m_RunElapsedTime += delta_time;
	}

	// Space를 누른 첫 프레임에 대시를 요청한다.
	if (InputKeyboard_IsTrigger(KK_SPACE))
	{
		GamePlayer::RequestDash();
	}

	// 전투 중인 방 안에서만 달리기를 막고, 복도에서는 빠르게 이동한다.
	const int player_room_index = ProceduralMap_GetRoomIndexAt(GamePlayer::GetPosition());
	const bool can_sprint = player_room_index < 0 || GameEnemy::IsRoomCleared(player_room_index);
	GamePlayer::SetSprinting(can_sprint);
	GamePlayer::Update(delta_time);
	GamePlayer::UpdateEmpoweredDashAttack(delta_time);
	m_CombatPresentation.ConsumeEnemyFeedback(m_Camera);
	if (TryStartBossDeathPresentation())
	{
		return;
	}
	const DirectX::XMFLOAT2 camera_target =
	    ProceduralMap_ClampCameraPosition(GetGameplayCameraTarget(), m_Camera.GetScreenSize());
	m_Camera.SetPosition(SmoothCameraFollow(m_Camera.GetPosition(), camera_target, delta_time));

	// 시간이 흐를 때만 적, 상자, 포탈 상태를 진행한다.
	if (!time_stopped)
	{
		GameEnemy::Update(delta_time);
		if (!m_BossIntro.HasPlayed() && GameEnemy::HasPendingBossSpawn())
		{
			if (m_BossIntro.Begin())
			{
				m_HasAutoAimTarget = false;
				m_ShowWorldMap = false;
				TimeStopEffect_Cancel();
				m_Camera.Shake(12.0f, 0.40f);
				return;
			}
		}
		SpawnClearedRoomRewardChests();
		for (const std::unique_ptr<Chest>& chest : m_RewardChests)
		{
			if (chest)
			{
				chest->Update(delta_time);
			}
		}
		UpdateRewardChestRoomLock();
		BulletType unlocked_weapon = BulletType::Count;
		if (GameBullet::ConsumeUnlockedWeapon(unlocked_weapon))
		{
			m_Hud.ShowWeaponUnlock(unlocked_weapon);
		}
		if (ProceduralMap_GetRoomIndexAt(GamePlayer::GetPosition()) == ProceduralMap_GetExitRoomIndex())
		{
			m_HasEnteredExitRoom = true;
		}
		m_RoundPortal.Update(delta_time, IsRoundExitOpen());

		if (InputKeyboard_IsTrigger(KK_F))
		{
			bool interacted = false;
			for (const std::unique_ptr<Chest>& chest : m_RewardChests)
			{
				if (chest && chest->CanInteract(GamePlayer::GetPosition()))
				{
					chest->Interact();
					interacted = true;
					break;
				}
			}
			if (!interacted && m_RoundPortal.CanInteract(GamePlayer::GetPosition()))
			{
				m_RoundPortal.Interact();
			}
		}
		if (m_RoundPortal.ConsumeActivation())
		{
			BeginRoundTransition();
			return;
		}
	}
	// 가장 가까운 적을 찾고 공격 가능한 무기가 있으면 자동 발사한다.
	m_HasAutoAimTarget = GameEnemy::FindNearestAlive(GamePlayer::GetPosition(), m_AutoAimTarget);
	if (m_HasAutoAimTarget)
	{
		GamePlayer::SetAimTarget(m_AutoAimTarget);
	}

	if (m_HasAutoAimTarget)
	{
		if (GameBullet::Fire(GamePlayer::GetPosition(), m_AutoAimTarget))
		{
			m_Camera.Shake(SceneConstants::CameraFeedback::PlayerFireShakeSize,
			               SceneConstants::CameraFeedback::PlayerFireShakeDuration);
		}
	}

	// 시간 정지 중에도 이미 발사한 플레이어 탄과 타격 연출은 진행한다.
	GameBullet::Update(delta_time);
	cGameEffectManager::GetInstance().Update(delta_time);
	GameDamageText::Update(delta_time);
	m_ChainLightning.Update(delta_time);

	// 위치 갱신이 모두 끝난 뒤 이번 프레임 충돌을 한 번 계산한다.
	CollisionSystem_Clear();
	GameBullet::RegisterColliders();
	GameEnemy::RegisterColliders();
	GamePlayer::RegisterCollider();
	CollisionSystem_Update();
	GameEnemy::HandleCollisionHits(m_ChainLightning);
	m_CombatPresentation.ConsumeEnemyFeedback(m_Camera);
	if (TryStartBossDeathPresentation())
	{
		return;
	}
	if (!time_stopped)
	{
		GameExperienceGem::Update(delta_time, GamePlayer::GetPosition());
		GameHealingItem::Update(delta_time, GamePlayer::GetPosition());
	}
	if (!time_stopped)
	{
		GamePlayer::HandleCollisionHits();
		float player_damage = 0.0f;
		if (GamePlayer::ConsumeDamageFeedback(player_damage) && GamePlayer::GetHitPoint() > 0.0f)
		{
			m_CombatPresentation.PresentPlayerDamage(player_damage, m_Camera);
		}
		if (GamePlayer::GetHitPoint() <= 0.0f)
		{
			m_ShowWorldMap = false;
			m_HasAutoAimTarget = false;
			m_CombatPresentation.BeginPlayerDeath(m_Camera);
			return;
		}
	}
}

void IngameScene::Draw()
{
	Sprite_SetFilter(kPOINT);

	// 월드 좌표로 그리는 것들. 순서가 곧 화면의 앞뒤 순서다.
	Sprite_SetViewMatrix(m_Camera.GetViewMatrix());
	SpriteInstanced_SetViewMatrix(m_Camera.GetViewMatrix());
	SpriteLighting_DisableRadialLight();
	ApplyWorldLighting(m_ChainLightning, m_RewardChests, IsRoundExitOpen(), m_Camera.GetPosition(),
	                   m_Camera.GetScreenSize());
	ProceduralMap_Draw(m_Camera.GetPosition(), m_Camera.GetScreenSize());
	cSlimeGoo::GetInstance().DrawGround();
	if (!m_BossIntro.IsActive())
	{
		GameEnemy::DrawSpawnTelegraphs();
	}
	Blood::Draw();
	m_RoundPortal.Draw();
	for (const std::unique_ptr<Chest>& chest : m_RewardChests)
	{
		if (chest)
		{
			chest->Draw();
		}
	}
	GameExperienceGem::Draw();
	GameHealingItem::Draw();
	if (!m_CombatPresentation.IsPlayerDeathActive() && !m_CombatPresentation.IsBossDeathActive() && !m_ShowWorldMap &&
	    m_TransitionState == RoundTransitionState::None && !TimeStopEffect_IsActive())
	{
		for (const std::unique_ptr<Chest>& chest : m_RewardChests)
		{
			if (chest)
			{
				m_Hud.DrawInteractionPrompt(*chest, GamePlayer::GetPosition(), m_RoundElapsedTime);
			}
		}
		m_Hud.DrawInteractionPrompt(m_RoundPortal, GamePlayer::GetPosition(), m_RoundElapsedTime);
	}
	if (!m_CombatPresentation.IsPlayerDeathActive())
	{
		GamePlayer::Draw();
	}
	GameEnemy::Draw();
	cSlimeGoo::GetInstance().DrawBurst();
	if (!m_CombatPresentation.IsBossDeathActive())
	{
		GameBullet::Draw();
	}
	cGameEffectManager::GetInstance().Draw();
	if (!m_CombatPresentation.IsBossDeathActive())
	{
		m_ChainLightning.Draw();
		GameDamageText::Draw();
	}
	ProceduralMap_DrawEncounterLock();
	if (!m_CombatPresentation.IsBossDeathActive())
	{
		GameEnemy::DrawProjectiles();
	}

	// 사망 연출은 월드 위에 조리개처럼 닫히며 UI는 그리지 않는다.
	if (m_CombatPresentation.IsPlayerDeathActive())
	{
		m_CombatPresentation.DrawPlayerDeath(m_DeathOverlayTextureID, m_Camera);
		return;
	}

	// 이 아래는 화면 좌표 UI라 카메라 행렬과 월드 조명을 끈다.
	SpriteLighting_DisableWorldLighting();
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	m_CombatPresentation.DrawScreenFlash();
	const ProceduralMapOverviewLayout overview = ProceduralMap_DrawOverview(
	    m_Camera.GetScreenSize(), m_ShowWorldMap, GameEnemy::IsRoomDiscovered, GameEnemy::IsRoomCleared);
	GamePlayer::DrawMapMarker(overview.Origin, overview.WorldScale, overview.IsExpanded);
	const DirectX::XMFLOAT2 auto_aim_screen_position = m_Camera.WorldToScreen(m_AutoAimTarget);
	m_Hud.Draw(m_QSkillCooldownRemaining, m_ShowWorldMap,
	           m_BossIntro.IsActive() || m_CombatPresentation.IsBossDeathActive(), m_HasAutoAimTarget,
	           auto_aim_screen_position);
	ProceduralMap_DrawFadeOverlay(m_Camera.GetScreenSize(), m_FadeAlpha);
	m_BossIntro.Draw(m_DeathOverlayTextureID);
	m_MenuController.Draw(m_DeathOverlayTextureID);
}

// 메뉴 선택 결과만 실제 게임 상태에 반영한다.
void IngameScene::HandlePauseAction(IngamePauseAction action)
{
	switch (action)
	{
	case IngamePauseAction::Resume:
		m_MenuController.ClosePause();
		break;
	case IngamePauseAction::Retry:
		SceneManager_ChangeScene(SceneID::Ingame);
		break;
	case IngamePauseAction::Title:
		SceneManager_ChangeScene(SceneID::Title);
		break;
	case IngamePauseAction::Exit:
		if (HWND window = GetActiveWindow())
		{
			PostMessage(window, WM_CLOSE, 0, 0);
		}
		else
		{
			PostQuitMessage(0);
		}
		break;
	case IngamePauseAction::None:
	default:
		break;
	}
}

void IngameScene::ApplyAugment(const IngameAugmentSelection& selection)
{
	// 강화 수치는 무기 시스템이 소유하므로 선택 결과만 전달한다.
	switch (selection.Choice)
	{
	case IngameAugmentChoice::MultiShot:
		GameBullet::IncreaseProjectileCount(selection.WeaponType);
		break;
	case IngameAugmentChoice::Overdrive:
		GameBullet::MultiplyAttackSpeed(selection.WeaponType, 1.20f);
		break;
	case IngameAugmentChoice::Power:
		GameBullet::MultiplyDamage(selection.WeaponType, 1.20f);
		break;
	case IngameAugmentChoice::None:
	default:
		return;
	}

	if (selection.Reason == IngameAugmentReason::RoundClear)
	{
		m_MenuController.CloseAugment();
		AdvanceToNextRound();
		return;
	}

	m_LastAugmentLevel = std::min(m_LastAugmentLevel + 1, GamePlayer::GetLevel());
	if (m_LastAugmentLevel < GamePlayer::GetLevel())
	{
		if (m_MenuController.OpenAugment())
		{
			return;
		}
		m_LastAugmentLevel = GamePlayer::GetLevel();
	}
	m_MenuController.CloseAugment();
}

// 보상 상자와 라운드 이동에 필요한 씬 흐름.
namespace
{

	DirectX::XMFLOAT2 GetRoundGameplayCameraTarget()
	{
		// 플레이어만 따라가면 방 끝이 잘리므로 방 중앙 쪽으로 조금 당긴다.
		const DirectX::XMFLOAT2 player_position = GamePlayer::GetPosition();
		const int room_index = ProceduralMap_GetRoomIndexAt(player_position);
		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room)
		{
			return player_position;
		}

		return {
			player_position.x + (room->Center.x - player_position.x) * SceneConstants::CameraFollow::RoomCenterWeight,
			player_position.y + (room->Center.y - player_position.y) * SceneConstants::CameraFollow::RoomCenterWeight
		};
	}

} // namespace

void IngameScene::ClearRewardChests()
{
	if (m_ChestLockedRoomIndex >= 0)
	{
		ProceduralMap_ClearEncounterLock(m_ChestLockedRoomIndex);
		m_ChestLockedRoomIndex = -1;
	}
	m_InitialWeaponChest = nullptr;
	m_RoundExitRewardChest = nullptr;
	for (const std::unique_ptr<Chest>& chest : m_RewardChests)
	{
		if (chest)
		{
			chest->Finalize();
		}
	}
	m_RewardChests.clear();
	m_RewardChestRoomIndices.clear();
}

void IngameScene::SpawnClearedRoomRewardChests()
{
	if (m_CurrentRound == GetMapData().GetTotalRoundCount())
	{
		return;
	}

	const int room_count = ProceduralMap_GetRoomCount();
	if (m_RewardedRooms.size() != static_cast<std::size_t>(room_count))
	{
		m_RewardedRooms.assign(room_count, false);
	}

	// 큰 방과 보스 방은 클리어한 순간 한 번만 보상 상자를 만든다.
	for (int room_index = 0; room_index < room_count; ++room_index)
	{
		if (m_RewardedRooms[room_index] || !GameEnemy::IsRoomCleared(room_index))
		{
			continue;
		}

		const ProceduralMapRoom* room = ProceduralMap_GetRoom(room_index);
		if (!room || (!room->IsLargeRoom && !room->IsBossRoom))
		{
			continue;
		}
		m_RewardedRooms[room_index] = true;
		std::unique_ptr<Chest> chest = std::make_unique<Chest>();
		chest->Initialize(room->Center);
		if (room_index == ProceduralMap_GetExitRoomIndex())
		{
			m_RoundExitRewardChest = chest.get();
		}
		m_RewardChests.push_back(std::move(chest));
		m_RewardChestRoomIndices.push_back(room_index);
	}
}

void IngameScene::UpdateRewardChestRoomLock()
{
	// 상자를 열기 전까지 다른 방으로 빠져나가지 못하게 문을 유지한다.
	int active_chest_room_index = -1;
	const std::size_t chest_count = std::min(m_RewardChests.size(), m_RewardChestRoomIndices.size());
	for (std::size_t i = 0; i < chest_count; ++i)
	{
		if (m_RewardChests[i] && !m_RewardChests[i]->IsGone())
		{
			active_chest_room_index = m_RewardChestRoomIndices[i];
			break;
		}
	}

	if (active_chest_room_index >= 0)
	{
		m_ChestLockedRoomIndex = active_chest_room_index;
		ProceduralMap_LockEncounterRoom(active_chest_room_index);
		return;
	}

	if (m_ChestLockedRoomIndex >= 0)
	{
		ProceduralMap_ClearEncounterLock(m_ChestLockedRoomIndex);
		m_ChestLockedRoomIndex = -1;
	}
}

void IngameScene::ResetRound(int global_round, bool regenerate_current)
{
	global_round = std::clamp(global_round, 1, GetMapData().GetTotalRoundCount());
	const bool restore_initial_weapon_chest =
	    global_round == 1 && m_CurrentRound == 1 && m_InitialWeaponChest && !m_InitialWeaponChest->IsGone();

	// 이전 맵에 묶인 오브젝트를 먼저 비운 다음 새 맵을 만든다.
	CollisionSystem_Clear();
	Blood::Clear();
	cSlimeGoo::GetInstance().Clear();
	GameBullet::Clear();
	GameDamageText::Clear();
	GameExperienceGem::Clear();
	GameHealingItem::Clear();
	cGameEffectManager::GetInstance().Clear();
	m_ChainLightning.Clear();
	ClearRewardChests();
	if (regenerate_current)
	{
		ProceduralMap_Regenerate();
	}
	else
	{
		ProceduralMap_GenerateRound(global_round);
	}
	// 맵 좌표가 바뀐 뒤 적과 플레이어 위치를 새 방 기준으로 맞춘다.
	GameEnemy::ResetDungeon();
	m_BossIntro.Reset();
	m_Hud.ResetBossPresentation();
	GamePlayer::SetPosition(ProceduralMap_GetPlayerSpawnPosition());
	m_RewardedRooms.assign(ProceduralMap_GetRoomCount(), false);
	if (restore_initial_weapon_chest)
	{
		std::unique_ptr<Chest> initial_weapon_chest = std::make_unique<Chest>();
		initial_weapon_chest->Initialize(ProceduralMap_GetPlayerSpawnPosition());
		m_InitialWeaponChest = initial_weapon_chest.get();
		m_RewardChests.push_back(std::move(initial_weapon_chest));
		m_RewardChestRoomIndices.push_back(ProceduralMap_GetStartRoomIndex());
		UpdateRewardChestRoomLock();
	}
	m_RoundPortal.Reset(ProceduralMap_GetRoundExitPosition());
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(GetRoundGameplayCameraTarget(), m_Camera.GetScreenSize()));
	m_Camera.SetZoom(1.0f);
	m_Camera.StopShake();
	m_Camera.StopZoomPunch();
	m_CurrentRound = global_round;
	Bgm_Play(GetRoundBgmTrack(m_CurrentRound));
	m_RoundElapsedTime = 0.0f;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	m_HasEnteredExitRoom = false;
	m_CombatPresentation.Reset();
}

void IngameScene::BeginRoundTransition()
{
	if (m_TransitionState != RoundTransitionState::None)
	{
		return;
	}
	m_ShowWorldMap = false;
	m_FadeAlpha = 0.0f;
	m_TransitionState = RoundTransitionState::FadingOut;
}

void IngameScene::AdvanceToNextRound()
{
	if (m_CurrentRound >= GetMapData().GetTotalRoundCount())
	{
		SceneManager_ShowClearScene(m_RunElapsedTime);
		return;
	}

	ResetRound(m_CurrentRound + 1, false);
	m_FadeAlpha = 1.0f;
	m_TransitionState = RoundTransitionState::FadingIn;
}

void IngameScene::UpdateRoundTransition(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	// 암전이 끝난 시점에 강화 선택창을 열고, 선택 후 다음 맵으로 넘어간다.
	if (m_TransitionState == RoundTransitionState::FadingOut)
	{
		m_FadeAlpha = std::min(1.0f, m_FadeAlpha + delta_time / SceneConstants::Transition::FadeOutDuration);
		if (m_FadeAlpha < 1.0f)
		{
			return;
		}
		if (m_CurrentRound < GetMapData().GetTotalRoundCount() &&
		    m_MenuController.OpenAugment(IngameAugmentReason::RoundClear))
		{
			return;
		}
		AdvanceToNextRound();
		return;
	}

	if (m_TransitionState == RoundTransitionState::FadingIn)
	{
		m_FadeAlpha = std::max(0.0f, m_FadeAlpha - delta_time / SceneConstants::Transition::FadeInDuration);
		if (m_FadeAlpha <= 0.0f)
		{
			m_TransitionState = RoundTransitionState::None;
		}
	}
}

bool IngameScene::IsRoundExitOpen() const
{
	const bool exit_reward_collected = !m_RoundExitRewardChest || m_RoundExitRewardChest->IsGone();
	return m_HasEnteredExitRoom && GameEnemy::IsRoundCleared() && exit_reward_collected;
}
