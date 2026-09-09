#include "Audio.h"
#include "Constants/presentation_constants.h"
#include "boss_intro_presentation.h"
#include "camera.h"
#include "chain_lightning.h"
#include "config.h"
#include "bitmap_text.h"
#include "direct3d.h"
#include "game_bullet.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "game_player.h"
#include "ingame_combat_presentation.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "random_utils.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "sprite_lighting.h"
#include "texture.h"
#include "time_stop_effect.h"
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace
{

	std::unique_ptr<hal::BitmapText> CreateBossNameText()
	{
		const char* name = GameEnemy::GetBossDisplayName();
		const std::size_t character_count = name ? std::char_traits<char>::length(name) : 0;

		const float text_width = character_count > 0 ? PresentationConstants::TextAndEffects::glyph_width +
		                                                   (static_cast<float>(character_count) - 1.0f) *
		                                                       PresentationConstants::TextAndEffects::character_spacing
		                                             : 0.0f;
		return std::make_unique<hal::BitmapText>(Direct3D_GetDevice(), Direct3D_GetDeviceContext(),
		                                         L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
		                                         SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_WIDTH * 0.5f - text_width * 0.5f,
		                                         SCREEN_HEIGHT * 0.5f -
		                                             PresentationConstants::TextAndEffects::glyph_height * 0.5f,
		                                         1, 0, PresentationConstants::TextAndEffects::glyph_height,
		                                         PresentationConstants::TextAndEffects::character_spacing);
	}

	void DrawDangerBand(int texture_id, float center_y, float elapsed_time, float direction, float alpha)
	{
		if (texture_id == TEXTURE_INVALID_ID || alpha <= 0.0f)
		{
			return;
		}
		const float tile_width = PresentationConstants::Intro::SourceWidth * PresentationConstants::Intro::BandHeight /
		                         PresentationConstants::Intro::SourceHeight;
		const float uv_phase =
		    std::fmod(std::max(elapsed_time, 0.0f) * PresentationConstants::Intro::ScrollSpeed / tile_width, 1.0f);
		SpriteInstance instance{};
		instance.Position = { SCREEN_WIDTH * 0.5f, center_y };
		instance.Size = { static_cast<float>(SCREEN_WIDTH), PresentationConstants::Intro::BandHeight };
		instance.Color = { 1.0f, 1.0f, 1.0f, alpha };
		instance.TexcoordOffset = {
			-direction * uv_phase,
			PresentationConstants::Intro::SourceY / PresentationConstants::Intro::TextureHeight,
		};
		instance.TexcoordScale = {
			SCREEN_WIDTH / tile_width,
			PresentationConstants::Intro::SourceHeight / PresentationConstants::Intro::TextureHeight,
		};
		SpriteInstanced_DrawWrapUUnlit(texture_id, &instance, 1);
	}
} // namespace

BossIntroPresentation::BossIntroPresentation() = default;

BossIntroPresentation::~BossIntroPresentation() = default;

bool BossIntroPresentation::Initialize()
{
	Finalize();
	m_DangerTextureID = Texture_Load(L"asset/texture/ui/boss/boss_danger_tape_generated.png", false);
	if (m_DangerTextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}
	m_WarningAudioID = Audio_Load("asset/sound/boss-warning-siren.wav");
	Reset();
	return true;
}

void BossIntroPresentation::Finalize()
{
	m_BossNameText.reset();
	Audio_Unload(m_WarningAudioID);
	m_WarningAudioID = -1;
	Texture_Release(m_DangerTextureID);
	m_DangerTextureID = TEXTURE_INVALID_ID;
	m_ElapsedTime = 0.0f;
	m_Active = false;
	m_Played = false;
}

void BossIntroPresentation::Reset()
{
	m_BossNameText = CreateBossNameText();
	m_ElapsedTime = 0.0f;
	m_Active = false;
	m_Played = false;
}

bool BossIntroPresentation::Begin()
{
	if (m_Played || m_Active)
	{
		return false;
	}
	m_Played = true;
	m_Active = true;
	m_ElapsedTime = 0.0f;
	Audio_Play(m_WarningAudioID);
	return true;
}

void BossIntroPresentation::Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (!m_Active)
	{
		return;
	}
	m_ElapsedTime += delta_time;
	if (m_ElapsedTime >= PresentationConstants::Intro::TotalDuration)
	{
		m_ElapsedTime = PresentationConstants::Intro::TotalDuration;
		m_Active = false;
	}
}

void BossIntroPresentation::Draw(int overlay_texture_id)
{
	if (!m_Active || m_DangerTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	const float elapsed = m_ElapsedTime;
	float visibility = 1.0f;
	if (elapsed < PresentationConstants::Intro::SlideInDuration)
	{
		visibility = SmoothStep(elapsed / PresentationConstants::Intro::SlideInDuration);
	}
	else if (elapsed > PresentationConstants::Intro::SlideOutStart)
	{
		visibility = 1.0f - SmoothStep((elapsed - PresentationConstants::Intro::SlideOutStart) /
		                               PresentationConstants::Intro::SlideOutDuration);
	}
	const float blink = 0.5f + 0.5f * std::sin(elapsed * PresentationConstants::Intro::BlinkCyclesPerSecond * 2.0f *
	                                           PresentationConstants::Intro::Pi);
	const float band_alpha = visibility * (0.72f + blink * 0.28f);
	const float offscreen_upper_y = -PresentationConstants::Intro::BandHeight * 0.5f - 16.0f;
	const float offscreen_lower_y = SCREEN_HEIGHT + PresentationConstants::Intro::BandHeight * 0.5f + 16.0f;
	const float upper_y =
	    offscreen_upper_y + (PresentationConstants::Intro::UpperTargetY - offscreen_upper_y) * visibility;
	const float lower_y =
	    offscreen_lower_y + (PresentationConstants::Intro::LowerTargetY - offscreen_lower_y) * visibility;
	Sprite_DrawSized(overlay_texture_id, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f, static_cast<float>(SCREEN_WIDTH),
	                 static_cast<float>(SCREEN_HEIGHT), { 0.34f, 0.01f, 0.025f, visibility * (0.12f + blink * 0.10f) });
	DrawDangerBand(m_DangerTextureID, upper_y, elapsed, 1.0f, band_alpha);
	DrawDangerBand(m_DangerTextureID, lower_y, elapsed, -1.0f, band_alpha);
	if (m_BossNameText)
	{
		const float name_fade = SmoothStep((elapsed - PresentationConstants::Intro::SlideInDuration * 0.65f) / 0.22f);
		m_BossNameText->Clear();
		m_BossNameText->SetText(GameEnemy::GetBossDisplayName(),
		                              { 1.0f, 0.88f, 0.72f, visibility * name_fade * (0.84f + blink * 0.16f) });
		m_BossNameText->Draw();
	}
}

bool BossIntroPresentation::IsActive() const
{
	return m_Active;
}

bool BossIntroPresentation::HasPlayed() const
{
	return m_Played;
}

namespace
{

	float GetSmallExplosionTime(int index)
	{
		const float amount =
		    PresentationConstants::BossDeath::SmallExplosionCount > 1
		        ? static_cast<float>(std::clamp(index, 0, PresentationConstants::BossDeath::SmallExplosionCount - 1)) /
		              static_cast<float>(PresentationConstants::BossDeath::SmallExplosionCount - 1)
		        : 1.0f;
		return PresentationConstants::BossDeath::ExplosionStartTime +
		       PresentationConstants::BossDeath::SmallExplosionSequenceDuration * std::pow(amount, 0.72f);
	}

	void DrawIrisOverlay(int texture_id, const DirectX::XMFLOAT2& viewport_size, const DirectX::XMFLOAT2& center,
	                     float radius)
	{
		if (texture_id == TEXTURE_INVALID_ID)
		{
			return;
		}

		const DirectX::XMFLOAT4 black{ 0.0f, 0.0f, 0.0f, 1.0f };
		if (radius <= 0.0f)
		{
			Sprite_DrawSized(texture_id, viewport_size.x * 0.5f, viewport_size.y * 0.5f, viewport_size.x,
			                 viewport_size.y, black);
			return;
		}

		// 가로 띠마다 원 바깥쪽만 그려 조리개가 닫히는 화면을 만든다.
		for (float band_top = 0.0f; band_top < viewport_size.y;
		     band_top += PresentationConstants::PlayerDeath::IrisBandHeight)
		{
			const float band_height =
			    std::min(PresentationConstants::PlayerDeath::IrisBandHeight, viewport_size.y - band_top);
			const float band_center_y = band_top + band_height * 0.5f;
			const float distance_y = band_center_y - center.y;
			float hole_half_width = 0.0f;
			if (std::abs(distance_y) < radius)
			{
				hole_half_width = std::sqrt(std::max(radius * radius - distance_y * distance_y, 0.0f));
			}

			const float hole_left = std::clamp(center.x - hole_half_width, 0.0f, viewport_size.x);
			const float hole_right = std::clamp(center.x + hole_half_width, 0.0f, viewport_size.x);
			if (hole_left > 0.0f)
			{
				Sprite_DrawSized(texture_id, hole_left * 0.5f, band_center_y, hole_left, band_height, black);
			}
			if (hole_right < viewport_size.x)
			{
				const float right_width = viewport_size.x - hole_right;
				Sprite_DrawSized(texture_id, hole_right + right_width * 0.5f, band_center_y, right_width, band_height,
				                 black);
			}
		}
	}
} // namespace

struct IngameCombatPresentation::Impl
{
	// 일반 피격 연출
	int ScreenFlashTextureID{ TEXTURE_INVALID_ID };
	float HitStopRemaining{ 0.0f };
	float ScreenFlashDuration{ 0.0f };
	float ScreenFlashRemaining{ 0.0f };
	DirectX::XMFLOAT4 ScreenFlashColor{ 1.0f, 1.0f, 1.0f, 0.0f };

	// 플레이어 사망 연출
	float PlayerDeathElapsedTime{ 0.0f };
	float PlayerDeathAnimationFinishedElapsed{ 0.0f };
	bool PlayerDeathActive{ false };
	bool PlayerDeathAnimationStarted{ false };

	// 보스 사망 연출
	std::array<int, 6> ExplosionAudioIDs{ -1, -1, -1, -1, -1, -1 };
	int FinalExplosionAudioID{ -1 };
	DirectX::XMFLOAT2 BossPosition{};
	DirectX::XMFLOAT2 BossDrawSize{};
	DirectX::XMFLOAT2 BossCameraStartPosition{};
	DirectX::XMFLOAT2 BossCameraTargetPosition{};
	float BossElapsedTime{ 0.0f };
	float BossCameraStartZoom{ 1.0f };
	int BossExplosionCount{ 0 };
	std::size_t BossExplosionAudioIndex{ 0 };
	bool BossDeathActive{ false };
	bool BossFinalExplosionPlayed{ false };
	bool BossImplosionPlayed{ false };

	void ResetCombat()
	{
		HitStopRemaining = 0.0f;
		ScreenFlashDuration = 0.0f;
		ScreenFlashRemaining = 0.0f;
		ScreenFlashColor = { 1.0f, 1.0f, 1.0f, 0.0f };
	}

	void ResetPlayerDeath()
	{
		PlayerDeathElapsedTime = 0.0f;
		PlayerDeathAnimationFinishedElapsed = 0.0f;
		PlayerDeathActive = false;
		PlayerDeathAnimationStarted = false;
	}

	void ResetBossDeath()
	{
		BossPosition = {};
		BossDrawSize = {};
		BossCameraStartPosition = {};
		BossCameraTargetPosition = {};
		BossElapsedTime = 0.0f;
		BossCameraStartZoom = 1.0f;
		BossExplosionCount = 0;
		BossExplosionAudioIndex = 0;
		BossDeathActive = false;
		BossFinalExplosionPlayed = false;
		BossImplosionPlayed = false;
	}

	void TriggerScreenFlash(const DirectX::XMFLOAT4& color, float duration)
	{
		if (duration <= 0.0f || color.w <= 0.0f)
		{
			return;
		}

		const float current_alpha =
		    ScreenFlashDuration > 0.0f ? ScreenFlashColor.w * (ScreenFlashRemaining / ScreenFlashDuration) : 0.0f;
		if (color.w >= current_alpha)
		{
			ScreenFlashColor = color;
		}
		ScreenFlashDuration = std::max(duration, 0.01f);
		ScreenFlashRemaining = ScreenFlashDuration;
	}
};

IngameCombatPresentation::IngameCombatPresentation() : m_Impl(std::make_unique<Impl>())
{
}

IngameCombatPresentation::~IngameCombatPresentation() = default;

bool IngameCombatPresentation::Initialize()
{
	Finalize();
	m_Impl->ScreenFlashTextureID = Texture_Load(L"asset/texture/white_square.png", false);
	for (int& audio_id : m_Impl->ExplosionAudioIDs)
	{
		audio_id = Audio_Load("asset/sound/mixkit-short-explosion-1694.wav");
	}
	m_Impl->FinalExplosionAudioID = Audio_Load("asset/sound/pixabay-shockwave-105526.wav");
	Reset();
	return m_Impl->ScreenFlashTextureID != TEXTURE_INVALID_ID && m_Impl->FinalExplosionAudioID >= 0;
}

void IngameCombatPresentation::Finalize()
{
	Texture_Release(m_Impl->ScreenFlashTextureID);
	m_Impl->ScreenFlashTextureID = TEXTURE_INVALID_ID;
	for (int& audio_id : m_Impl->ExplosionAudioIDs)
	{
		Audio_Unload(audio_id);
		audio_id = -1;
	}
	Audio_Unload(m_Impl->FinalExplosionAudioID);
	m_Impl->FinalExplosionAudioID = -1;
	Reset();
}

void IngameCombatPresentation::Reset()
{
	m_Impl->ResetCombat();
	m_Impl->ResetPlayerDeath();
	m_Impl->ResetBossDeath();
}

bool IngameCombatPresentation::UpdateHitStop(float delta_time)
{
	const float safe_delta_time = std::max(delta_time, 0.0f);
	m_Impl->ScreenFlashRemaining = std::max(m_Impl->ScreenFlashRemaining - safe_delta_time, 0.0f);

	const bool was_hit_stopped = m_Impl->HitStopRemaining > 0.0f;
	m_Impl->HitStopRemaining = std::max(m_Impl->HitStopRemaining - safe_delta_time, 0.0f);
	return was_hit_stopped;
}

void IngameCombatPresentation::ConsumeEnemyFeedback(cCamera& camera)
{
	GameEnemy::CombatFeedback feedback{};
	if (!GameEnemy::ConsumeCombatFeedback(feedback))
	{
		return;
	}

	// 같은 프레임에 여러 적을 맞혀도 흔들림이 지나치게 커지지 않게 완만하게 증가시킨다.
	const float hit_cluster = std::sqrt(static_cast<float>(std::max(feedback.HitCount, 1)));
	float shake_size = 0.0f;
	float shake_duration = 0.045f + std::min(hit_cluster, 4.0f) * 0.008f;
	float hit_stop = 0.0f;
	float zoom_punch = 0.002f + std::min(hit_cluster, 4.0f) * 0.001f;

	if (feedback.HeavyHit)
	{
		shake_size += 0.85f;
		hit_stop = std::max(hit_stop, 0.012f);
		zoom_punch += 0.006f;
	}
	if (feedback.KillCount > 0)
	{
		shake_size += 1.25f + std::min(feedback.KillCount, 4) * 0.35f;
		shake_duration += 0.035f;
		if (feedback.KillCount >= 2)
		{
			hit_stop = std::max(hit_stop, 0.018f);
		}
		zoom_punch += 0.010f;
	}
	if (feedback.BossHit)
	{
		shake_size += 0.35f;
	}
	if (feedback.BossKilled)
	{
		shake_size = 8.0f;
		shake_duration = 0.20f;
		hit_stop = 0.055f;
		zoom_punch = 0.035f;
	}

	if (shake_size > 0.0f)
	{
		camera.Shake(std::min(shake_size, 9.0f), shake_duration);
	}
	camera.PunchZoom(std::min(zoom_punch, 0.04f), shake_duration + 0.04f);
	m_Impl->HitStopRemaining = std::max(m_Impl->HitStopRemaining, hit_stop);
}

void IngameCombatPresentation::PresentPlayerDamage(float damage, cCamera& camera)
{
	const float amount = Saturate(damage / 20.0f);
	camera.Shake(11.0f + amount * 7.0f, 0.26f);
	camera.PunchZoom(0.045f + amount * 0.025f, 0.22f);
	m_Impl->HitStopRemaining = std::max(m_Impl->HitStopRemaining, 0.055f + amount * 0.025f);
	m_Impl->TriggerScreenFlash({ 1.0f, 0.015f, 0.01f, 0.32f + amount * 0.14f }, 0.20f);
}

void IngameCombatPresentation::DrawScreenFlash() const
{
	if (m_Impl->ScreenFlashTextureID == TEXTURE_INVALID_ID || m_Impl->ScreenFlashRemaining <= 0.0f ||
	    m_Impl->ScreenFlashDuration <= 0.0f)
	{
		return;
	}

	const float amount = Saturate(m_Impl->ScreenFlashRemaining / m_Impl->ScreenFlashDuration);
	DirectX::XMFLOAT4 color = m_Impl->ScreenFlashColor;
	color.w *= amount * amount;
	Sprite_DrawSized(m_Impl->ScreenFlashTextureID, SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f,
	                 static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), color);
}

bool IngameCombatPresentation::BeginPlayerDeath(cCamera& camera)
{
	if (m_Impl->PlayerDeathActive)
	{
		return false;
	}

	m_Impl->ResetPlayerDeath();
	m_Impl->PlayerDeathActive = true;
	TimeStopEffect_Cancel();
	GamePlayer::PrepareDeathSequence();
	camera.SetPosition(GamePlayer::GetPosition());
	camera.SetZoom(1.0f);
	camera.Shake(PresentationConstants::PlayerDeath::ShakeSize, PresentationConstants::PlayerDeath::ShakeDuration);
	return true;
}

bool IngameCombatPresentation::UpdatePlayerDeath(float delta_time, cCamera& camera)
{
	if (!m_Impl->PlayerDeathActive)
	{
		return false;
	}

	const float safe_delta_time = std::max(delta_time, 0.0f);
	m_Impl->PlayerDeathElapsedTime += safe_delta_time;
	camera.SetPosition(GamePlayer::GetPosition());
	const float zoom_progress =
	    Saturate(m_Impl->PlayerDeathElapsedTime / PresentationConstants::PlayerDeath::ZoomDuration);
	camera.SetZoom(1.0f + (PresentationConstants::PlayerDeath::Zoom - 1.0f) * SmoothStep(zoom_progress));

	if (!m_Impl->PlayerDeathAnimationStarted &&
	    m_Impl->PlayerDeathElapsedTime >=
	        PresentationConstants::PlayerDeath::IrisDelay + PresentationConstants::PlayerDeath::IrisDuration)
	{
		m_Impl->PlayerDeathAnimationStarted = true;
		GamePlayer::BeginDeathAnimation();
	}
	if (m_Impl->PlayerDeathAnimationStarted)
	{
		GamePlayer::UpdateDeathAnimation(delta_time);
	}
	if (GamePlayer::IsDeathAnimationFinished())
	{
		m_Impl->PlayerDeathAnimationFinishedElapsed += safe_delta_time;
	}
	return m_Impl->PlayerDeathAnimationFinishedElapsed >= PresentationConstants::PlayerDeath::AfterAnimationHold;
}

void IngameCombatPresentation::DrawPlayerDeath(int overlay_texture_id, cCamera& camera) const
{
	if (!m_Impl->PlayerDeathActive)
	{
		return;
	}

	const DirectX::XMFLOAT2 viewport_size = camera.GetScreenSize();
	const DirectX::XMFLOAT2 player_screen_position = camera.WorldToScreen(GamePlayer::GetPosition());
	const float iris_progress =
	    Saturate((m_Impl->PlayerDeathElapsedTime - PresentationConstants::PlayerDeath::IrisDelay) /
	             PresentationConstants::PlayerDeath::IrisDuration);
	const float initial_radius = Length(viewport_size) * 0.5f + 48.0f;

	SpriteLighting_DisableWorldLighting();
	Sprite_ResetViewMatrix();
	SpriteInstanced_SetViewMatrix(DirectX::XMMatrixIdentity());
	DrawIrisOverlay(overlay_texture_id, viewport_size, player_screen_position,
	                initial_radius * (1.0f - SmoothStep(iris_progress)));

	Sprite_SetViewMatrix(camera.GetViewMatrix());
	GamePlayer::Draw();
	Sprite_ResetViewMatrix();
}

bool IngameCombatPresentation::IsPlayerDeathActive() const
{
	return m_Impl->PlayerDeathActive;
}

bool IngameCombatPresentation::TryBeginBossDeath(cCamera& camera, cChainLightning& chain_lightning)
{
	if (m_Impl->BossDeathActive)
	{
		return false;
	}

	GameEnemy::BossDefeatedEvent event{};
	if (!GameEnemy::ConsumeBossDefeatedEvent(event))
	{
		return false;
	}

	m_Impl->ResetBossDeath();
	m_Impl->BossDeathActive = true;
	m_Impl->BossPosition = event.Position;
	m_Impl->BossDrawSize = {
		std::max(event.DrawSize.x, 160.0f),
		std::max(event.DrawSize.y, 160.0f),
	};
	m_Impl->BossCameraStartPosition = camera.GetPosition();
	m_Impl->BossCameraTargetPosition = ProceduralMap_ClampCameraPosition(m_Impl->BossPosition, camera.GetScreenSize());
	m_Impl->BossCameraStartZoom = camera.GetZoom();
	// 연출이 시작되면 화면에 남은 공격과 일시 정지를 정리한다.
	TimeStopEffect_Cancel();
	GameBullet::Clear();
	chain_lightning.Clear();
	camera.StopShake();
	camera.Shake(7.0f, 0.16f);
	camera.PunchZoom(0.045f, 0.22f);
	m_Impl->HitStopRemaining = 0.0f;
	m_Impl->TriggerScreenFlash({ 1.0f, 0.86f, 0.46f, 0.24f }, 0.16f);
	return true;
}

void IngameCombatPresentation::UpdateBossDeath(float delta_time, cCamera& camera)
{
	if (!m_Impl->BossDeathActive)
	{
		return;
	}

	const float previous_elapsed_time = m_Impl->BossElapsedTime;
	m_Impl->BossElapsedTime += std::max(delta_time, 0.0f);
	if (m_Impl->BossElapsedTime < PresentationConstants::BossDeath::ZoomInDuration)
	{
		const float amount = SmoothStep(m_Impl->BossElapsedTime / PresentationConstants::BossDeath::ZoomInDuration);
		camera.SetPosition(Lerp(m_Impl->BossCameraStartPosition, m_Impl->BossCameraTargetPosition, amount));
		camera.SetZoom(m_Impl->BossCameraStartZoom +
		               (PresentationConstants::BossDeath::Zoom - m_Impl->BossCameraStartZoom) * amount);
	}
	else if (m_Impl->BossElapsedTime >= PresentationConstants::BossDeath::ZoomOutStartTime)
	{
		if (previous_elapsed_time < PresentationConstants::BossDeath::ZoomOutStartTime)
		{
			camera.Shake(PresentationConstants::BossDeath::ZoomOutShakeSize,
			             PresentationConstants::BossDeath::ZoomOutDuration);
		}
		const float amount = SmoothStep((m_Impl->BossElapsedTime - PresentationConstants::BossDeath::ZoomOutStartTime) /
		                                PresentationConstants::BossDeath::ZoomOutDuration);
		camera.SetPosition(Lerp(m_Impl->BossCameraTargetPosition, m_Impl->BossCameraStartPosition, amount));
		camera.SetZoom(PresentationConstants::BossDeath::Zoom +
		               (m_Impl->BossCameraStartZoom - PresentationConstants::BossDeath::Zoom) * amount);
	}
	else
	{
		camera.SetPosition(m_Impl->BossCameraTargetPosition);
		camera.SetZoom(PresentationConstants::BossDeath::Zoom);
	}

	cGameEffectManager& effects = cGameEffectManager::GetInstance();
	while (m_Impl->BossExplosionCount < PresentationConstants::BossDeath::SmallExplosionCount &&
	       m_Impl->BossElapsedTime >= GetSmallExplosionTime(m_Impl->BossExplosionCount))
	{
		const float angle = Random01() * DirectX::XM_2PI;
		const float radius = std::sqrt(Random01());
		const DirectX::XMFLOAT2 explosion_position{
			m_Impl->BossPosition.x + std::cos(angle) * radius * m_Impl->BossDrawSize.x * 0.43f,
			m_Impl->BossPosition.y + std::sin(angle) * radius * m_Impl->BossDrawSize.y * 0.39f,
		};
		const float boss_scale =
		    std::clamp(std::min(m_Impl->BossDrawSize.x, m_Impl->BossDrawSize.y) / 320.0f, 0.72f, 1.20f);
		effects.Play(GameEffectType::BossDeathSmallExplosion, explosion_position,
		             boss_scale * (0.76f + Random01() * 0.48f), { 1.0f, 0.84f + Random01() * 0.16f, 0.72f, 1.0f },
		             Random01() * DirectX::XM_2PI);

		const int audio_id =
		    m_Impl->ExplosionAudioIDs[m_Impl->BossExplosionAudioIndex % m_Impl->ExplosionAudioIDs.size()];
		Audio_Play(audio_id);
		++m_Impl->BossExplosionAudioIndex;
		++m_Impl->BossExplosionCount;
		camera.Shake(PresentationConstants::BossDeath::SmallExplosionShakeSize,
		             PresentationConstants::BossDeath::SmallExplosionShakeDuration);
		camera.PunchZoom(0.012f, 0.10f);
		m_Impl->TriggerScreenFlash({ 1.0f, 0.52f, 0.10f, 0.10f }, 0.085f);
	}

	if (!m_Impl->BossImplosionPlayed &&
	    m_Impl->BossElapsedTime >=
	        PresentationConstants::BossDeath::FinalExplosionTime - PresentationConstants::BossDeath::ImplosionLeadTime)
	{
		m_Impl->BossImplosionPlayed = true;
		camera.StopShake();
		camera.PunchZoom(0.085f, PresentationConstants::BossDeath::ImplosionLeadTime);
		effects.Play(GameEffectType::VoidImplosion, m_Impl->BossPosition, 2.15f, { 0.72f, 0.38f, 1.0f, 1.0f });
		m_Impl->TriggerScreenFlash({ 0.36f, 0.08f, 0.72f, 0.24f }, PresentationConstants::BossDeath::ImplosionLeadTime);
	}

	if (!m_Impl->BossFinalExplosionPlayed &&
	    m_Impl->BossElapsedTime >= PresentationConstants::BossDeath::FinalExplosionTime)
	{
		m_Impl->BossFinalExplosionPlayed = true;
		const float final_radius = std::max(m_Impl->BossDrawSize.x, m_Impl->BossDrawSize.y) * 0.72f;
		effects.PlayAreaExplosion(m_Impl->BossPosition, final_radius);
		effects.Play(GameEffectType::ElectricImpact, m_Impl->BossPosition, 2.55f, { 1.0f, 0.96f, 0.78f, 1.0f },
		             Random01() * DirectX::XM_2PI);

		for (int i = 0; i < PresentationConstants::TextAndEffects::FinalRingExplosionCount; ++i)
		{
			const float angle = DirectX::XM_2PI * static_cast<float>(i) /
			                        static_cast<float>(PresentationConstants::TextAndEffects::FinalRingExplosionCount) +
			                    Random01() * 0.14f;
			const float distance = final_radius * (0.42f + Random01() * 0.16f);
			const DirectX::XMFLOAT2 ring_position{
				m_Impl->BossPosition.x + std::cos(angle) * distance,
				m_Impl->BossPosition.y + std::sin(angle) * distance,
			};
			effects.Play(GameEffectType::BossDeathSmallExplosion, ring_position, 0.82f + Random01() * 0.54f,
			             { 1.0f, 0.72f, 0.28f, 1.0f }, angle);
		}

		Audio_Play(m_Impl->FinalExplosionAudioID);
		camera.Shake(PresentationConstants::BossDeath::FinalExplosionShakeSize,
		             PresentationConstants::BossDeath::FinalExplosionShakeDuration);
		camera.PunchZoom(0.13f, 0.46f);
		m_Impl->TriggerScreenFlash({ 1.0f, 0.98f, 0.84f, 0.82f }, 0.28f);
		GameEnemy::CompleteBossDefeat();
	}

	if (m_Impl->BossElapsedTime >= PresentationConstants::BossDeath::EndTime)
	{
		m_Impl->BossDeathActive = false;
		camera.SetPosition(m_Impl->BossCameraStartPosition);
		camera.SetZoom(m_Impl->BossCameraStartZoom);
	}
}

bool IngameCombatPresentation::IsBossDeathActive() const
{
	return m_Impl->BossDeathActive;
}
