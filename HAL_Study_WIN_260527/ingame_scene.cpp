#include "ingame_scene.h"

#include "Audio.h"
#include "blood.h"
#include "config.h"
#include "collision.h"
#include "game_bullet.h"
#include "game_damage_text.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "game_experience_gem.h"
#include "game_healing_item.h"
#include "game_data_manager.h"
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
	namespace TimeStopAudio
	{
		constexpr const char* ActivatePath =
			"asset/sound/leohpaz-88-teleport-02.wav";
	}

	namespace CameraFeedback
	{
		constexpr float PlayerFireShakeSize = 5.0f;
		constexpr float PlayerFireShakeDuration = 0.08f;
		constexpr float EmpoweredDashShakeSize = 11.0f;
		constexpr float EmpoweredDashShakeDuration = 0.16f;
	}

	namespace EmpoweredDashAttack
	{
		constexpr float SlashHalfWidth = 46.0f;
		constexpr float DamagePerHit = 8.0f;
	}

	namespace RoundTransition
	{
		constexpr float FadeOutDuration = 0.40f;
		constexpr float FadeInDuration = 0.45f;
	}

	namespace CameraFollow
	{
		constexpr float RoomCenterWeight = 0.35f;
		constexpr float SmoothSpeed = 9.0f;
	}

	namespace DeathSequence
	{
		constexpr float ShakeSize = 22.0f;
		constexpr float ShakeDuration = 0.65f;
		constexpr float Zoom = 2.4f;
		constexpr float ZoomDuration = 1.35f;
		constexpr float IrisDelay = 0.20f;
		constexpr float IrisDuration = 1.35f;
		constexpr float IrisBandHeight = 8.0f;
		constexpr float AfterAnimationHold = 0.25f;
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
					0.86f,
					{ 1.0f, 0.96f, 0.78f },
					0.16f,
					270.0f,
					0.18f,
					{ 1.0f, 0.92f, 0.70f },
				};
			}

			return {
				0.52f,
				{ 0.78f, 0.86f, 1.0f },
				0.14f,
				330.0f,
				0.88f,
				{ 1.0f, 0.70f, 0.38f },
			};
		}
	}

	void ApplyWorldLighting(
		const cChainLightning& chain_lightning,
		const std::vector<std::unique_ptr<Chest>>& reward_chests,
		bool portal_open,
		const DirectX::XMFLOAT2& camera_position,
		const DirectX::XMFLOAT2& viewport_size)
	{
		const WorldLighting::Profile profile =
			WorldLighting::GetActiveProfile();
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
		light_count = chain_lightning.AppendPointLights(
			lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY);
		light_count = GameEnemy::AppendPointLights(
			lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY,
			camera_position, viewport_size);
		light_count = cGameEffectManager::GetInstance().AppendPointLights(
			lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY);
		light_count = ProceduralMap_AppendTorchLights(
			lights.data(), light_count, SPRITE_POINT_LIGHT_CAPACITY,
			camera_position, viewport_size);

		SpriteLighting_SetWorldLighting(
			profile.AmbientBrightness,
			profile.DirectLightColor,
			profile.DirectLightStrength,
			lights.data(),
			light_count);
	}

	void DisableWorldLighting()
	{
		SpriteLighting_DisableWorldLighting();
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
			player_position.x +
				(room->Center.x - player_position.x) * CameraFollow::RoomCenterWeight,
			player_position.y +
				(room->Center.y - player_position.y) * CameraFollow::RoomCenterWeight };
	}

	DirectX::XMFLOAT2 SmoothCameraFollow(
		const DirectX::XMFLOAT2& current,
		const DirectX::XMFLOAT2& target,
		float delta_time)
	{
		const float follow_ratio = 1.0f - std::exp(
			-CameraFollow::SmoothSpeed * std::max(delta_time, 0.0f));
		return {
			current.x + (target.x - current.x) * follow_ratio,
			current.y + (target.y - current.y) * follow_ratio };
	}















	void DrawDeathIrisOverlay(
		int texture_id,
		const DirectX::XMFLOAT2& viewport_size,
		const DirectX::XMFLOAT2& center,
		float radius)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}

		const DirectX::XMFLOAT4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
		if (radius <= 0.0f)
		{
			Sprite_DrawSized(
				texture_id,
				viewport_size.x * 0.5f,
				viewport_size.y * 0.5f,
				viewport_size.x,
				viewport_size.y,
				black);
			return;
		}

		for (float band_top = 0.0f;
			band_top < viewport_size.y;
			band_top += DeathSequence::IrisBandHeight)
		{
			const float band_height = std::min(
				DeathSequence::IrisBandHeight,
				viewport_size.y - band_top);
			const float band_center_y = band_top + band_height * 0.5f;
			const float distance_y = band_center_y - center.y;
			float hole_half_width = 0.0f;
			if (std::abs(distance_y) < radius)
			{
				hole_half_width = std::sqrt(
					std::max(radius * radius - distance_y * distance_y, 0.0f));
			}

			const float hole_left = std::clamp(
				center.x - hole_half_width, 0.0f, viewport_size.x);
			const float hole_right = std::clamp(
				center.x + hole_half_width, 0.0f, viewport_size.x);
			if (hole_left > 0.0f)
			{
				Sprite_DrawSized(
					texture_id,
					hole_left * 0.5f,
					band_center_y,
					hole_left,
					band_height,
					black);
			}
			if (hole_right < viewport_size.x)
			{
				const float right_width = viewport_size.x - hole_right;
				Sprite_DrawSized(
					texture_id,
					hole_right + right_width * 0.5f,
					band_center_y,
					right_width,
					band_height,
					black);
			}
		}
	}
}

IngameScene::~IngameScene() = default;

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
	if (m_CurrentRound ==
		GameDataManager::GetInstance().GetMapGameData().GetTotalRoundCount())
	{
		return;
	}

	const int room_count = ProceduralMap_GetRoomCount();
	if (m_RewardedRooms.size() != static_cast<std::size_t>(room_count))
	{
		m_RewardedRooms.assign(room_count, false);
	}

	for (int room_index = 0; room_index < room_count; ++room_index)
	{
		if (m_RewardedRooms[room_index] ||
			!GameEnemy::IsRoomCleared(room_index))
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
	int active_chest_room_index = -1;
	const std::size_t chest_count = std::min(
		m_RewardChests.size(), m_RewardChestRoomIndices.size());
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

void IngameScene::ResetRound(
	int global_round,
	bool regenerate_current)
{
	global_round = std::clamp(
		global_round,
		1,
		GameDataManager::GetInstance().GetMapGameData().GetTotalRoundCount());
	const bool restore_initial_weapon_chest =
		global_round == 1 &&
		m_CurrentRound == 1 &&
		m_InitialWeaponChest &&
		!m_InitialWeaponChest->IsGone();

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
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(
		GetGameplayCameraTarget(), m_Camera.GetScreenSize()));
	m_CurrentRound = global_round;
	m_RoundElapsedTime = 0.0f;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	m_HasEnteredExitRoom = false;
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
	if (m_CurrentRound >=
		GameDataManager::GetInstance().GetMapGameData().GetTotalRoundCount())
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
	if (m_TransitionState == RoundTransitionState::FadingOut)
	{
		m_FadeAlpha = std::min(
			1.0f, m_FadeAlpha + delta_time / RoundTransition::FadeOutDuration);
		if (m_FadeAlpha < 1.0f)
		{
			return;
		}
		// Reward selection will be inserted before this advance later.
		AdvanceToNextRound();
		return;
	}

	if (m_TransitionState == RoundTransitionState::FadingIn)
	{
		m_FadeAlpha = std::max(
			0.0f, m_FadeAlpha - delta_time / RoundTransition::FadeInDuration);
		if (m_FadeAlpha <= 0.0f)
		{
			m_TransitionState = RoundTransitionState::None;
		}
	}
}

bool IngameScene::IsRoundExitOpen() const
{
	const bool exit_reward_collected =
		!m_RoundExitRewardChest || m_RoundExitRewardChest->IsGone();
	return m_HasEnteredExitRoom &&
		GameEnemy::IsRoundCleared() &&
		exit_reward_collected;
}


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

	m_LastAugmentLevel = std::min(
		m_LastAugmentLevel + 1,
		GamePlayer::GetLevel());
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

void IngameScene::BeginDeathSequence()
{
	if (m_IsDeathSequenceActive)
	{
		return;
	}

	m_IsDeathSequenceActive = true;
	m_DeathElapsedTime = 0.0f;
	m_DeathAnimationFinishedElapsed = 0.0f;
	m_HasDeathAnimationStarted = false;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	TimeStopEffect_Cancel();
	GamePlayer::PrepareDeathSequence();
	m_Camera.SetPosition(GamePlayer::GetPosition());
	m_Camera.SetZoom(1.0f);
	m_Camera.Shake(DeathSequence::ShakeSize, DeathSequence::ShakeDuration);
}

void IngameScene::UpdateDeathSequence(float delta_time)
{
	m_DeathElapsedTime += std::max(delta_time, 0.0f);
	m_Camera.SetPosition(GamePlayer::GetPosition());

	const float zoom_progress = std::clamp(
		m_DeathElapsedTime / DeathSequence::ZoomDuration, 0.0f, 1.0f);
	const float smooth_zoom =
		zoom_progress * zoom_progress * (3.0f - 2.0f * zoom_progress);
	m_Camera.SetZoom(1.0f + (DeathSequence::Zoom - 1.0f) * smooth_zoom);

	const float iris_end_time = DeathSequence::IrisDelay + DeathSequence::IrisDuration;
	if (!m_HasDeathAnimationStarted && m_DeathElapsedTime >= iris_end_time)
	{
		m_HasDeathAnimationStarted = true;
		GamePlayer::BeginDeathAnimation();
	}
	if (m_HasDeathAnimationStarted)
	{
		GamePlayer::UpdateDeathAnimation(delta_time);
	}
	if (GamePlayer::IsDeathAnimationFinished())
	{
		m_DeathAnimationFinishedElapsed += std::max(delta_time, 0.0f);
	}
	if (m_DeathAnimationFinishedElapsed >= DeathSequence::AfterAnimationHold)
	{
		SceneManager_ChangeScene(SceneID::GameOver);
	}
}


bool IngameScene::Initialize()
{
	if (!SpriteInstanced_Initialize())
	{
		return false;
	}
	if (!ProceduralMap_Initialize())
	{
		SpriteInstanced_Finalize();
		return false;
	}
	m_DeathOverlayTextureID = Texture_Load(
		L"asset/texture/map/dungeon/structure/void_deep.png", false);
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

	m_Camera.SetScreenSize((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
	m_Camera.SetZoom(1.0f);

	GamePlayer::Initialize();
	GameExperienceGem::Initialize();
	GameHealingItem::Initialize();
	GamePlayer::SetPosition(ProceduralMap_GetPlayerSpawnPosition());
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(
		GetGameplayCameraTarget(), m_Camera.GetScreenSize()));
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
	m_TimeStopActivateAudioID = LoadAudio(TimeStopAudio::ActivatePath);
	CollisionSystem_Initialize();
	if (!m_Hud.Initialize(m_Camera.GetScreenSize()) ||
		!m_MenuController.Initialize() ||
		!m_BossIntro.Initialize())
	{
		Finalize();
		return false;
	}
	m_CurrentRound = 1;
	m_FadeAlpha = 1.0f;
	m_DeathElapsedTime = 0.0f;
	m_DeathAnimationFinishedElapsed = 0.0f;
	m_TransitionState = RoundTransitionState::FadingIn;
	m_IsDeathSequenceActive = false;
	m_HasDeathAnimationStarted = false;
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
	TimeStopEffect_Cancel();
	if (m_TimeStopActivateAudioID >= 0)
	{
		UnloadAudio(m_TimeStopActivateAudioID);
		m_TimeStopActivateAudioID = -1;
	}
	InputMouse_SetVisible(true);
	m_BossIntro.Finalize();
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
	m_HasAutoAimTarget = false;
	if (m_Hud.IsWeaponUnlockPopupOpen())
	{
		m_Hud.UpdateWeaponUnlockPopup();
		return;
	}
	ProceduralMap_Update(delta_time);
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
	if (!m_IsDeathSequenceActive &&
		GamePlayer::GetLevel() > m_LastAugmentLevel)
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
	if (!m_IsDeathSequenceActive && InputKeyboard_IsTrigger(KK_ESCAPE))
	{
		m_HasAutoAimTarget = false;
		m_MenuController.OpenPause();
		return;
	}
	m_Camera.Update(delta_time);
	Blood::Update(delta_time);
	cSlimeGoo::GetInstance().Update(delta_time);
	if (m_IsDeathSequenceActive)
	{
		UpdateDeathSequence(delta_time);
		return;
	}
	if (InputKeyboard_IsTrigger(KK_P) &&
		m_TransitionState == RoundTransitionState::None)
	{
		TimeStopEffect_Cancel();
		m_ShowWorldMap = false;
		AdvanceToNextRound();
		return;
	}
	m_QSkillCooldownRemaining = std::max(
		0.0f,
		m_QSkillCooldownRemaining - delta_time);
	TimeStopEffect_Update(delta_time);
	if (InputKeyboard_IsTrigger(KK_Q) && !TimeStopEffect_IsActive() &&
		m_QSkillCooldownRemaining <= 0.0f &&
		m_TransitionState == RoundTransitionState::None && !m_ShowWorldMap)
	{
		m_QSkillCooldownRemaining = IngameHud::GetQSkillCooldownDuration();
		TimeStopEffect_Trigger(m_Camera.WorldToScreen(GamePlayer::GetPosition()));
		if (m_TimeStopActivateAudioID >= 0)
		{
			PlayAudio(m_TimeStopActivateAudioID);
		}
		GamePlayer::BeginEmpoweredDashMode();
		m_Camera.Shake(5.0f, 0.22f);
	}
	const bool time_stopped = TimeStopEffect_IsActive();
	if (!time_stopped)
	{
		GamePlayer::EndEmpoweredDashMode();
	}

	if (!time_stopped && m_TransitionState != RoundTransitionState::None)
	{
		UpdateRoundTransition(delta_time);
		return;
	}

	if (!time_stopped && InputKeyboard_IsTrigger(KK_M))
	{
		m_ShowWorldMap = !m_ShowWorldMap;
	}

	if (!time_stopped && InputKeyboard_IsTrigger(KK_R))
	{
		ResetRound(m_CurrentRound, true);
		m_FadeAlpha = 1.0f;
		m_TransitionState = RoundTransitionState::FadingIn;
		return;
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

	// Keep the existing instant dash on the initial Space press.
	if (InputKeyboard_IsTrigger(KK_SPACE))
	{
		GamePlayer::RequestDash();
	}

	// Fast running is the default traversal movement everywhere except inside
	// an uncleared room (corridors remain valid traversal space).
	const int player_room_index = ProceduralMap_GetRoomIndexAt(
		GamePlayer::GetPosition());
	const bool can_sprint = player_room_index < 0 ||
		GameEnemy::IsRoomCleared(player_room_index);
	GamePlayer::SetSprinting(can_sprint);
	GamePlayer::Update(delta_time);
	DirectX::XMFLOAT2 slash_start{};
	DirectX::XMFLOAT2 slash_end{};
	if (GamePlayer::ConsumeEmpoweredDashAttack(slash_start, slash_end))
	{
		GameEnemy::ApplyDashSlashDamage(
			slash_start,
			slash_end,
			EmpoweredDashAttack::SlashHalfWidth,
			EmpoweredDashAttack::DamagePerHit);
		m_Camera.Shake(
			CameraFeedback::EmpoweredDashShakeSize,
			CameraFeedback::EmpoweredDashShakeDuration);
	}
	GameEnemy::UpdateDashSlashAttacks(delta_time);
	const DirectX::XMFLOAT2 camera_target = ProceduralMap_ClampCameraPosition(
		GetGameplayCameraTarget(), m_Camera.GetScreenSize());
	m_Camera.SetPosition(SmoothCameraFollow(
		m_Camera.GetPosition(), camera_target, delta_time));

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
		if (ProceduralMap_GetRoomIndexAt(GamePlayer::GetPosition()) ==
			ProceduralMap_GetExitRoomIndex())
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
			if (!interacted &&
				m_RoundPortal.CanInteract(GamePlayer::GetPosition()))
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
	m_HasAutoAimTarget = GameEnemy::FindNearestAlive(
		GamePlayer::GetPosition(), m_AutoAimTarget);
	if (m_HasAutoAimTarget)
	{
		GamePlayer::SetAimTarget(m_AutoAimTarget);
	}

	if (m_HasAutoAimTarget)
	{
		if (GameBullet::Fire(GamePlayer::GetPosition(), m_AutoAimTarget))
		{
			m_Camera.Shake(
				CameraFeedback::PlayerFireShakeSize,
				CameraFeedback::PlayerFireShakeDuration);
		}
	}

	// Player bullets and their feedback remain active; enemy simulation does not.
	GameBullet::Update(delta_time);
	cGameEffectManager::GetInstance().Update(delta_time);
	GameDamageText::Update(delta_time);
	m_ChainLightning.Update(delta_time);

	CollisionSystem_Clear();
	GameBullet::RegisterColliders();
	GameEnemy::RegisterColliders();
	GamePlayer::RegisterCollider();
	CollisionSystem_Update();
	GameEnemy::HandleCollisionHits(m_ChainLightning);
	if (!time_stopped)
	{
		GameExperienceGem::Update(delta_time, GamePlayer::GetPosition());
		GameHealingItem::Update(delta_time, GamePlayer::GetPosition());
	}
	if (!time_stopped)
	{
		GamePlayer::HandleCollisionHits();
		if (GamePlayer::GetHitPoint() <= 0.0f)
		{
			BeginDeathSequence();
			return;
		}
	}

}
void IngameScene::Draw()
{
	Sprite_SetFilter(kPOINT);

	Sprite_SetViewMatrix(m_Camera.GetViewMatrix());
	SpriteInstanced_SetViewMatrix(m_Camera.GetViewMatrix());
	SpriteLighting_DisableRadialLight();
	ApplyWorldLighting(
		m_ChainLightning,
		m_RewardChests,
		IsRoundExitOpen(),
		m_Camera.GetPosition(),
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
	if (!m_IsDeathSequenceActive && !m_ShowWorldMap &&
		m_TransitionState == RoundTransitionState::None &&
		!TimeStopEffect_IsActive())
	{
		for (const std::unique_ptr<Chest>& chest : m_RewardChests)
		{
			if (chest)
			{
				m_Hud.DrawInteractionPrompt(
					*chest,
					GamePlayer::GetPosition(),
					m_RoundElapsedTime);
			}
		}
		m_Hud.DrawInteractionPrompt(
			m_RoundPortal,
			GamePlayer::GetPosition(),
			m_RoundElapsedTime);
	}
	if (!m_IsDeathSequenceActive)
	{
		GamePlayer::Draw();
	}
	GameEnemy::Draw();
	cSlimeGoo::GetInstance().DrawBurst();
	GameBullet::Draw();
	cGameEffectManager::GetInstance().Draw();
	m_ChainLightning.Draw();
	GameDamageText::Draw();
	ProceduralMap_DrawEncounterLock();
	GameEnemy::DrawProjectiles();

	if (m_IsDeathSequenceActive)
	{
		const DirectX::XMFLOAT2 viewport_size = m_Camera.GetScreenSize();
		const DirectX::XMFLOAT2 player_screen_position =
			m_Camera.WorldToScreen(GamePlayer::GetPosition());
		const float iris_progress = std::clamp(
			(m_DeathElapsedTime - DeathSequence::IrisDelay) / DeathSequence::IrisDuration,
			0.0f,
			1.0f);
		const float smooth_iris =
			iris_progress * iris_progress * (3.0f - 2.0f * iris_progress);
		const float initial_radius =
			std::sqrt(viewport_size.x * viewport_size.x +
				viewport_size.y * viewport_size.y) * 0.5f + 48.0f;
		const float iris_radius = initial_radius * (1.0f - smooth_iris);

		DisableWorldLighting();
		Sprite_ResetViewMatrix();
		SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
		DrawDeathIrisOverlay(
			m_DeathOverlayTextureID,
			viewport_size,
			player_screen_position,
			iris_radius);

		Sprite_SetViewMatrix(m_Camera.GetViewMatrix());
		GamePlayer::Draw();
		Sprite_ResetViewMatrix();
		return;
	}

	DisableWorldLighting();
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	const ProceduralMapOverviewLayout overview = ProceduralMap_DrawOverview(
		m_Camera.GetScreenSize(),
		m_ShowWorldMap,
		GameEnemy::IsRoomDiscovered,
		GameEnemy::IsRoomCleared);
	GamePlayer::DrawMapMarker(
		overview.Origin, overview.WorldScale, overview.IsExpanded);
	const DirectX::XMFLOAT2 auto_aim_screen_position =
		m_Camera.WorldToScreen(m_AutoAimTarget);
	m_Hud.Draw(
		m_QSkillCooldownRemaining,
		m_ShowWorldMap,
		m_BossIntro.IsActive(),
		m_HasAutoAimTarget,
		auto_aim_screen_position);
	ProceduralMap_DrawFadeOverlay(m_Camera.GetScreenSize(), m_FadeAlpha);
	m_BossIntro.Draw(m_DeathOverlayTextureID);
	m_MenuController.Draw(m_DeathOverlayTextureID);
}
