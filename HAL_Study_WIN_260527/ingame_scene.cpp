#include "ingame_scene.h"

#include "blood.h"
#include "config.h"
#include "collision.h"
#include "game_bullet.h"
#include "game_damage_text.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "game_experience_gem.h"
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

#include <algorithm>
#include <array>
#include <cmath>

static constexpr float PLAYER_FIRE_INTERVAL = 0.08f;
static constexpr float PLAYER_FIRE_CAMERA_SHAKE_SIZE = 5.0f;
static constexpr float PLAYER_FIRE_CAMERA_SHAKE_DURATION = 0.08f;
static constexpr float ROUND_FADE_OUT_DURATION = 0.40f;
static constexpr float ROUND_FADE_IN_DURATION = 0.45f;
static constexpr int TOTAL_ROUND_COUNT = 4;
static constexpr float CROSSHAIR_DRAW_SIZE = 72.0f;
static constexpr float HEALTH_BAR_SCALE = 4.0f;
static constexpr float HEALTH_BAR_MARGIN = 28.0f;
static constexpr int HEALTH_BAR_TEXTURE_WIDTH = 50;
static constexpr int HEALTH_BAR_TEXTURE_HEIGHT = 9;
static constexpr float EXPERIENCE_BAR_GAP = 8.0f;
static constexpr float DEATH_SHAKE_SIZE = 22.0f;
static constexpr float DEATH_SHAKE_DURATION = 0.65f;
static constexpr float DEATH_ZOOM = 2.4f;
static constexpr float DEATH_ZOOM_DURATION = 1.35f;
static constexpr float DEATH_IRIS_DELAY = 0.20f;
static constexpr float DEATH_IRIS_DURATION = 1.35f;
static constexpr float DEATH_IRIS_BAND_HEIGHT = 8.0f;
static constexpr float DEATH_AFTER_ANIMATION_HOLD = 0.25f;
static constexpr float TEST_CHEST_SPAWN_OFFSET_X = 240.0f;
static constexpr float INTERACTION_PROMPT_DRAW_SIZE = 52.0f;
static constexpr float INTERACTION_PROMPT_BOB_AMOUNT = 5.0f;
static constexpr float INTERACTION_PROMPT_BOB_SPEED = 6.0f;

namespace
{
	DirectX::XMFLOAT2 GetTestChestSpawnPosition()
	{
		const DirectX::XMFLOAT2 player_spawn =
			ProceduralMap_GetPlayerSpawnPosition();
		return { player_spawn.x + TEST_CHEST_SPAWN_OFFSET_X, player_spawn.y };
	}

	void DrawInteractionPrompt(
		int texture_id,
		const IInteractable& interactable,
		const DirectX::XMFLOAT2& player_position,
		float elapsed_time)
	{
		if (texture_id == TEXTURE_INVALID_ID ||
			!interactable.CanInteract(player_position))
		{
			return;
		}

		DirectX::XMFLOAT2 prompt_position =
			interactable.GetInteractionPromptPosition();
		prompt_position.y += std::sin(elapsed_time * INTERACTION_PROMPT_BOB_SPEED) *
			INTERACTION_PROMPT_BOB_AMOUNT;
		Sprite_DrawSized(
			texture_id,
			prompt_position.x,
			prompt_position.y,
			INTERACTION_PROMPT_DRAW_SIZE,
			INTERACTION_PROMPT_DRAW_SIZE);
	}

	void DrawHealthBar(int empty_texture_id, int filled_texture_id)
	{
		const float draw_width = HEALTH_BAR_TEXTURE_WIDTH * HEALTH_BAR_SCALE;
		const float draw_height = HEALTH_BAR_TEXTURE_HEIGHT * HEALTH_BAR_SCALE;
		const float center_x = HEALTH_BAR_MARGIN + draw_width * 0.5f;
		const float center_y = HEALTH_BAR_MARGIN + draw_height * 0.5f;

		Sprite_DrawSized(
			empty_texture_id,
			center_x,
			center_y,
			draw_width,
			draw_height);

		const float max_hit_point = GamePlayer::GetMaxHitPoint();
		const float health_ratio = max_hit_point > 0.0f ?
			std::clamp(GamePlayer::GetHitPoint() / max_hit_point, 0.0f, 1.0f) : 0.0f;
		const int filled_texture_width = static_cast<int>(
			HEALTH_BAR_TEXTURE_WIDTH * health_ratio + 0.5f);
		if (filled_texture_width <= 0)
		{
			return;
		}

		const float filled_draw_width = filled_texture_width * HEALTH_BAR_SCALE;
		Sprite_DrawRegion(
			filled_texture_id,
			HEALTH_BAR_MARGIN + filled_draw_width * 0.5f,
			center_y,
			filled_draw_width,
			draw_height,
			0,
			0,
			filled_texture_width,
			HEALTH_BAR_TEXTURE_HEIGHT,
			{ 1.0f, 1.0f, 1.0f, 1.0f });
	}

	void DrawExperienceBar(int empty_texture_id, int filled_texture_id)
	{
		const float draw_width = HEALTH_BAR_TEXTURE_WIDTH * HEALTH_BAR_SCALE;
		const float draw_height = HEALTH_BAR_TEXTURE_HEIGHT * HEALTH_BAR_SCALE;
		const float top = HEALTH_BAR_MARGIN + draw_height + EXPERIENCE_BAR_GAP;
		const float center_x = HEALTH_BAR_MARGIN + draw_width * 0.5f;
		const float center_y = top + draw_height * 0.5f;

		Sprite_DrawSized(
			empty_texture_id,
			center_x,
			center_y,
			draw_width,
			draw_height);

		const int required_experience = GamePlayer::GetExperienceToNextLevel();
		const float experience_ratio = required_experience > 0 ?
			std::clamp(
				static_cast<float>(GamePlayer::GetExperience()) /
					static_cast<float>(required_experience),
				0.0f,
				1.0f) : 0.0f;
		const int filled_texture_width = static_cast<int>(
			HEALTH_BAR_TEXTURE_WIDTH * experience_ratio + 0.5f);
		if (filled_texture_width <= 0)
		{
			return;
		}

		const float filled_draw_width = filled_texture_width * HEALTH_BAR_SCALE;
		Sprite_DrawRegion(
			filled_texture_id,
			HEALTH_BAR_MARGIN + filled_draw_width * 0.5f,
			center_y,
			filled_draw_width,
			draw_height,
			0,
			0,
			filled_texture_width,
			HEALTH_BAR_TEXTURE_HEIGHT,
			{ 1.0f, 1.0f, 1.0f, 1.0f });
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
			band_top += DEATH_IRIS_BAND_HEIGHT)
		{
			const float band_height = std::min(
				DEATH_IRIS_BAND_HEIGHT,
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

void IngameScene::ResetRound(
	int global_round,
	bool regenerate_current)
{
	global_round = std::clamp(global_round, 1, TOTAL_ROUND_COUNT);

	CollisionSystem_Clear();
	Blood::Clear();
	cSlimeGoo::GetInstance().Clear();
	GameBullet::Clear();
	GameDamageText::Clear();
	GameExperienceGem::Clear();
	cGameEffectManager::GetInstance().Clear();
	m_ChainLightning.Clear();
	if (regenerate_current)
	{
		ProceduralMap_Regenerate();
	}
	else
	{
		ProceduralMap_GenerateRound(global_round);
	}
	GameEnemy::ResetDungeon();
	GamePlayer::SetPosition(ProceduralMap_GetPlayerSpawnPosition());
	m_TestChest.Reset(GetTestChestSpawnPosition());
	m_RoundPortal.Reset(ProceduralMap_GetRoundExitPosition());
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(
		GamePlayer::GetPosition(), m_Camera.GetScreenSize()));
	m_CurrentRound = global_round;
	m_FireCooldown = 0.0f;
	m_RoundElapsedTime = 0.0f;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
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

void IngameScene::UpdateRoundTransition(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (m_TransitionState == RoundTransitionState::FadingOut)
	{
		m_FadeAlpha = std::min(
			1.0f, m_FadeAlpha + delta_time / ROUND_FADE_OUT_DURATION);
		if (m_FadeAlpha < 1.0f)
		{
			return;
		}
		if (m_CurrentRound >= TOTAL_ROUND_COUNT)
		{
			SceneManager_ShowClearScene(m_RunElapsedTime);
			return;
		}

		// Reward selection will be inserted between these two fade phases later.
		ResetRound(m_CurrentRound + 1, false);
		m_FadeAlpha = 1.0f;
		m_TransitionState = RoundTransitionState::FadingIn;
		return;
	}

	if (m_TransitionState == RoundTransitionState::FadingIn)
	{
		m_FadeAlpha = std::max(
			0.0f, m_FadeAlpha - delta_time / ROUND_FADE_IN_DURATION);
		if (m_FadeAlpha <= 0.0f)
		{
			m_TransitionState = RoundTransitionState::None;
		}
	}
}

bool IngameScene::IsRoundExitOpen() const
{
	return GameEnemy::IsRoundCleared();
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
	m_Camera.Shake(DEATH_SHAKE_SIZE, DEATH_SHAKE_DURATION);
}

void IngameScene::UpdateDeathSequence(float delta_time)
{
	m_DeathElapsedTime += std::max(delta_time, 0.0f);
	m_Camera.SetPosition(GamePlayer::GetPosition());

	const float zoom_progress = std::clamp(
		m_DeathElapsedTime / DEATH_ZOOM_DURATION, 0.0f, 1.0f);
	const float smooth_zoom =
		zoom_progress * zoom_progress * (3.0f - 2.0f * zoom_progress);
	m_Camera.SetZoom(1.0f + (DEATH_ZOOM - 1.0f) * smooth_zoom);

	const float iris_end_time = DEATH_IRIS_DELAY + DEATH_IRIS_DURATION;
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
	if (m_DeathAnimationFinishedElapsed >= DEATH_AFTER_ANIMATION_HOLD)
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
	m_CrosshairTextureID = Texture_Load(L"asset/texture/crosshair.png", false);
	if (m_CrosshairTextureID == TEXTURE_INVALID_ID)
	{
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	m_HealthBarEmptyTextureID = Texture_Load(
		L"asset/fantasy_pixelart_ui/special-bars/life_bar_empty.png", false);
	m_HealthBarFilledTextureID = Texture_Load(
		L"asset/fantasy_pixelart_ui/special-bars/life_bar_filled.png", false);
	m_ExperienceBarEmptyTextureID = Texture_Load(
		L"asset/fantasy_pixelart_ui/special-bars/magic_bar_empty.png", false);
	m_ExperienceBarFilledTextureID = Texture_Load(
		L"asset/fantasy_pixelart_ui/special-bars/magic_bar_filled.png", false);
	m_DeathOverlayTextureID = Texture_Load(
		L"asset/texture/map/structure/void_deep.png", false);
	if (m_HealthBarEmptyTextureID == TEXTURE_INVALID_ID ||
		m_HealthBarFilledTextureID == TEXTURE_INVALID_ID ||
		m_ExperienceBarEmptyTextureID == TEXTURE_INVALID_ID ||
		m_ExperienceBarFilledTextureID == TEXTURE_INVALID_ID ||
		m_DeathOverlayTextureID == TEXTURE_INVALID_ID)
	{
		Texture_Release(m_DeathOverlayTextureID);
		Texture_Release(m_HealthBarEmptyTextureID);
		Texture_Release(m_HealthBarFilledTextureID);
		Texture_Release(m_ExperienceBarEmptyTextureID);
		Texture_Release(m_ExperienceBarFilledTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		m_HealthBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_HealthBarFilledTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarFilledTextureID = TEXTURE_INVALID_ID;
		Texture_Release(m_CrosshairTextureID);
		m_CrosshairTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	if (!Blood::Initialize())
	{
		Texture_Release(m_DeathOverlayTextureID);
		Texture_Release(m_HealthBarEmptyTextureID);
		Texture_Release(m_HealthBarFilledTextureID);
		Texture_Release(m_ExperienceBarEmptyTextureID);
		Texture_Release(m_ExperienceBarFilledTextureID);
		Texture_Release(m_CrosshairTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		m_HealthBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_HealthBarFilledTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarFilledTextureID = TEXTURE_INVALID_ID;
		m_CrosshairTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}
	if (!cSlimeGoo::GetInstance().Initialize())
	{
		Blood::Finalize();
		Texture_Release(m_DeathOverlayTextureID);
		Texture_Release(m_HealthBarEmptyTextureID);
		Texture_Release(m_HealthBarFilledTextureID);
		Texture_Release(m_ExperienceBarEmptyTextureID);
		Texture_Release(m_ExperienceBarFilledTextureID);
		Texture_Release(m_CrosshairTextureID);
		m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
		m_HealthBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_HealthBarFilledTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarEmptyTextureID = TEXTURE_INVALID_ID;
		m_ExperienceBarFilledTextureID = TEXTURE_INVALID_ID;
		m_CrosshairTextureID = TEXTURE_INVALID_ID;
		ProceduralMap_Finalize();
		SpriteInstanced_Finalize();
		return false;
	}

	m_Camera.SetScreenSize((float)SCREEN_WIDTH, (float)SCREEN_HEIGHT);
	m_Camera.SetZoom(1.0f);

	GamePlayer::Initialize();
	GameExperienceGem::Initialize();
	GamePlayer::SetPosition(ProceduralMap_GetPlayerSpawnPosition());
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(
		GamePlayer::GetPosition(), m_Camera.GetScreenSize()));
	GameBullet::Initialize();
	GameEnemy::Initialize();
	cGameEffectManager::GetInstance().Initialize();
	m_TestChest.Initialize(GetTestChestSpawnPosition());
	m_RoundPortal.Initialize(ProceduralMap_GetRoundExitPosition());
	m_InteractionPromptTextureID = Texture_Load(
		L"asset/texture/ui/input/key-f.png", false);
	GameDamageText::Initialize();
	m_ChainLightning.Initialize();
	CollisionSystem_Initialize();
	m_CurrentRound = 1;
	m_FadeAlpha = 1.0f;
	m_DeathElapsedTime = 0.0f;
	m_DeathAnimationFinishedElapsed = 0.0f;
	m_TransitionState = RoundTransitionState::FadingIn;
	m_IsDeathSequenceActive = false;
	m_HasDeathAnimationStarted = false;
	m_FireCooldown = 0.0f;
	m_RoundElapsedTime = 0.0f;
	m_RunElapsedTime = 0.0f;
	m_ShowWorldMap = false;
	m_HasAutoAimTarget = false;
	TimeStopEffect_Cancel();
	InputMouse_SetVisible(false);

	return true;
}

void IngameScene::Finalize()
{
	TimeStopEffect_Cancel();
	InputMouse_SetVisible(true);
	CollisionSystem_Finalize();
	m_ChainLightning.Finalize();
	GameDamageText::Finalize();
	cGameEffectManager::GetInstance().Finalize();
	m_RoundPortal.Finalize();
	m_TestChest.Finalize();
	Texture_Release(m_InteractionPromptTextureID);
	m_InteractionPromptTextureID = TEXTURE_INVALID_ID;
	GameEnemy::Finalize();
	GameBullet::Finalize();
	GameExperienceGem::Finalize();
	GamePlayer::Finalize();
	cSlimeGoo::GetInstance().Finalize();
	Blood::Finalize();
	ProceduralMap_Finalize();
	Texture_Release(m_DeathOverlayTextureID);
	Texture_Release(m_HealthBarEmptyTextureID);
	Texture_Release(m_HealthBarFilledTextureID);
	Texture_Release(m_ExperienceBarEmptyTextureID);
	Texture_Release(m_ExperienceBarFilledTextureID);
	m_DeathOverlayTextureID = TEXTURE_INVALID_ID;
	m_HealthBarEmptyTextureID = TEXTURE_INVALID_ID;
	m_HealthBarFilledTextureID = TEXTURE_INVALID_ID;
	m_ExperienceBarEmptyTextureID = TEXTURE_INVALID_ID;
	m_ExperienceBarFilledTextureID = TEXTURE_INVALID_ID;
	Texture_Release(m_CrosshairTextureID);
	m_CrosshairTextureID = TEXTURE_INVALID_ID;
	SpriteInstanced_Finalize();
}

void IngameScene::Update(float delta_time)
{
	m_HasAutoAimTarget = false;
	m_Camera.Update(delta_time);
	Blood::Update(delta_time);
	cSlimeGoo::GetInstance().Update(delta_time);
	if (m_IsDeathSequenceActive)
	{
		UpdateDeathSequence(delta_time);
		return;
	}
	TimeStopEffect_Update(delta_time);
	if (InputKeyboard_IsTrigger(KK_Q) && !TimeStopEffect_IsActive() &&
		m_TransitionState == RoundTransitionState::None && !m_ShowWorldMap)
	{
		TimeStopEffect_Trigger(m_Camera.WorldToScreen(GamePlayer::GetPosition()));
		m_Camera.Shake(5.0f, 0.22f);
	}
	const bool time_stopped = TimeStopEffect_IsActive();

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

	// Player-controlled time keeps running even while the world is stopped.
	if (InputKeyboard_IsTrigger(KK_SPACE))
	{
		GamePlayer::RequestDash();
	}
	if (InputKeyboard_IsTrigger(KK_D1) || InputKeyboard_IsTrigger(KK_NUMPAD1))
	{
		GameBullet::SetType(BulletType::Fireball);
	}
	else if (InputKeyboard_IsTrigger(KK_D2) || InputKeyboard_IsTrigger(KK_NUMPAD2))
	{
		GameBullet::SetType(BulletType::Lightning);
	}
	else if (InputKeyboard_IsTrigger(KK_D3) || InputKeyboard_IsTrigger(KK_NUMPAD3))
	{
		GameBullet::SetType(BulletType::Piercing);
	}
	else if (InputKeyboard_IsTrigger(KK_D4) || InputKeyboard_IsTrigger(KK_NUMPAD4))
	{
		GameBullet::SetType(BulletType::All);
	}
	GamePlayer::Update(delta_time);
	m_Camera.SetPosition(ProceduralMap_ClampCameraPosition(
		GamePlayer::GetPosition(), m_Camera.GetScreenSize()));

	if (!time_stopped)
	{
		GameEnemy::Update(delta_time);
		m_TestChest.Update(delta_time);
		m_RoundPortal.Update(delta_time, IsRoundExitOpen());

		if (InputKeyboard_IsTrigger(KK_F))
		{
			const std::array<IInteractable*, 2> interactables = {
				&m_TestChest,
				&m_RoundPortal,
			};
			for (IInteractable* interactable : interactables)
			{
				if (interactable->CanInteract(GamePlayer::GetPosition()))
				{
					interactable->Interact();
					break;
				}
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

	m_FireCooldown -= delta_time;
	if (m_HasAutoAimTarget && m_FireCooldown <= 0.0f)
	{
		GameBullet::Fire(GamePlayer::GetPosition(), GamePlayer::GetAimDirection());
		m_Camera.Shake(
			PLAYER_FIRE_CAMERA_SHAKE_SIZE,
			PLAYER_FIRE_CAMERA_SHAKE_DURATION);
		m_FireCooldown = PLAYER_FIRE_INTERVAL;
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
	SpriteInstanced_DisableRadialLight();
	SpriteInstanced_DisableDirectLight();
	SpriteInstanced_DisablePointLights();
	ProceduralMap_Draw(m_Camera.GetPosition(), m_Camera.GetScreenSize());
	m_RoundPortal.Draw();
	cSlimeGoo::GetInstance().DrawGround();
	GameEnemy::DrawSpawnTelegraphs();
	m_TestChest.Draw();
	GameExperienceGem::Draw();
	if (!m_IsDeathSequenceActive && !m_ShowWorldMap &&
		m_TransitionState == RoundTransitionState::None &&
		!TimeStopEffect_IsActive())
	{
		const std::array<const IInteractable*, 2> interactables = {
			&m_TestChest,
			&m_RoundPortal,
		};
		for (const IInteractable* interactable : interactables)
		{
			DrawInteractionPrompt(
				m_InteractionPromptTextureID,
				*interactable,
				GamePlayer::GetPosition(),
				m_RoundElapsedTime);
		}
	}
	if (!m_IsDeathSequenceActive)
	{
		GamePlayer::Draw();
	}
	Blood::Draw();
	GameEnemy::Draw();
	cSlimeGoo::GetInstance().DrawBurst();
	GameBullet::Draw();
	cGameEffectManager::GetInstance().Draw();
	m_ChainLightning.Draw();
	GameDamageText::Draw();
	ProceduralMap_DrawEncounterLock();

	if (m_IsDeathSequenceActive)
	{
		const DirectX::XMFLOAT2 viewport_size = m_Camera.GetScreenSize();
		const DirectX::XMFLOAT2 player_screen_position =
			m_Camera.WorldToScreen(GamePlayer::GetPosition());
		const float iris_progress = std::clamp(
			(m_DeathElapsedTime - DEATH_IRIS_DELAY) / DEATH_IRIS_DURATION,
			0.0f,
			1.0f);
		const float smooth_iris =
			iris_progress * iris_progress * (3.0f - 2.0f * iris_progress);
		const float initial_radius =
			std::sqrt(viewport_size.x * viewport_size.x +
				viewport_size.y * viewport_size.y) * 0.5f + 48.0f;
		const float iris_radius = initial_radius * (1.0f - smooth_iris);

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

	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	const ProceduralMapOverviewLayout overview = ProceduralMap_DrawOverview(
		m_Camera.GetPosition(),
		m_Camera.GetScreenSize(),
		m_ShowWorldMap);
	GameEnemy::DrawMapMarkers(
		overview.Origin, overview.WorldScale, overview.IsExpanded);
	GamePlayer::DrawMapMarker(
		overview.Origin, overview.WorldScale, overview.IsExpanded);
	DrawHealthBar(m_HealthBarEmptyTextureID, m_HealthBarFilledTextureID);
	DrawExperienceBar(m_ExperienceBarEmptyTextureID, m_ExperienceBarFilledTextureID);
	ProceduralMap_DrawFadeOverlay(m_Camera.GetScreenSize(), m_FadeAlpha);

	if (m_HasAutoAimTarget)
	{
		const DirectX::XMFLOAT2 target_screen_position =
			m_Camera.WorldToScreen(m_AutoAimTarget);
		const float crosshair_x = std::clamp(
			target_screen_position.x, 0.0f, static_cast<float>(SCREEN_WIDTH));
		const float crosshair_y = std::clamp(
			target_screen_position.y, 0.0f, static_cast<float>(SCREEN_HEIGHT));
		Sprite_DrawSized(
			m_CrosshairTextureID,
			crosshair_x,
			crosshair_y,
			CROSSHAIR_DRAW_SIZE,
			CROSSHAIR_DRAW_SIZE);
	}
}
