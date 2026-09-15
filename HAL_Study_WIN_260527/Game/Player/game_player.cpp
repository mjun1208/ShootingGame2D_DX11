#include "game_player.h"
#include "Constants/player_constants.h"

#include "Audio.h"
#include "blood.h"
#include "collision.h"
#include "direct3d.h"
#include "game_damage_text.h"
#include "game_data_manager.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "input_keyboard.h"
#include "math_utils.h"
#include "procedural_map.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include <algorithm>
#include <array>
#include <cmath>

#include <DirectXMath.h>
using namespace DirectX;

static int g_PlayerIdleTextureID = TEXTURE_INVALID_ID;
static int g_PlayerRunTextureID = TEXTURE_INVALID_ID;
static int g_PlayerDieTextureID = TEXTURE_INVALID_ID;

static XMFLOAT2 g_PlayerPos;
static XMFLOAT2 g_PlayerAimDirection = { 0.0f, -1.0f };

static float g_PlayerHitPoint = PlayerConstants::Stats::MaxHitPoint;
static float g_PlayerMaxHitPoint = PlayerConstants::Stats::MaxHitPoint;
static float g_PlayerDamageInvincibilityTimer = 0.0f;
static float g_PlayerDamageFeedback = 0.0f;
static int g_PlayerLevel = 1;
static int g_PlayerExperience = 0;

static float g_PlayerSpeed = PlayerConstants::Stats::MoveSpeed;

static int g_PlayerCurrentFrame = 0;
static float g_PlayerAnimTimer = 0.0f;
static bool g_PlayerIsMoving = false;
static bool g_PlayerFacingLeft = false;
static int g_PlayerDieCurrentFrame = 0;
static float g_PlayerDieAnimTimer = 0.0f;
static bool g_PlayerDieAnimationVisible = false;
static bool g_PlayerDieAnimationFinished = false;
static float g_PlayerDashCooldown = 0.0f;
static float g_PlayerDashElapsed = PlayerConstants::Dash::Normal.Duration;
static float g_PlayerDashEffectTime = 0.0f;
static XMFLOAT2 g_PlayerDashStartPos = { 0.0f, 0.0f };
static XMFLOAT2 g_PlayerDashEndPos = { 0.0f, 0.0f };
static XMFLOAT2 g_PlayerDashDirection = { 0.0f, 0.0f };
static bool g_PlayerDashRequested = false;
static bool g_PlayerDashActive = false;
static bool g_PlayerDashIsEmpowered = false;
static bool g_PlayerDashEffectIsEmpowered = false;
static bool g_PlayerIsSprinting = false;
static float g_PlayerSprintDustElapsed = 0.0f;
static bool g_PlayerEmpoweredDashModeActive = false;
static int g_PlayerEmpoweredDashCharges = 0;
static bool g_PlayerEmpoweredDashAttackPending = false;
static XMFLOAT2 g_PlayerEmpoweredDashAttackStart = { 0.0f, 0.0f };
static XMFLOAT2 g_PlayerEmpoweredDashAttackEnd = { 0.0f, 0.0f };
static int g_PlayerDashAudioID = -1;
static int g_PlayerEmpoweredDashAudioID = -1;
static int g_PlayerDamageAudioID = -1;
static int g_PlayerLevelUpAudioID = -1;
static int g_PlayerSlashArcTextureID = TEXTURE_INVALID_ID;
static int g_PlayerSlashChainTextureID = TEXTURE_INVALID_ID;
static int g_PlayerSlashFinishTextureID = TEXTURE_INVALID_ID;
static int g_PlayerSlashLaserTextureID = TEXTURE_INVALID_ID;

struct PlayerSlashTrail
{
	XMFLOAT2 Start{};
	XMFLOAT2 End{};
	float Elapsed{ 0.0f };
	bool Active{ false };
};

static std::array<PlayerSlashTrail, PlayerConstants::Slash::TrailMax> g_PlayerSlashTrails{};
static int g_PlayerSlashTrailReplaceIndex = 0;

static int GetRequiredExperience(int level)
{
	return GameDataManager::GetInstance().GetPlayerLevelGameData().GetExperienceToNextLevel(level);
}

static void ResetDashState()
{
	g_PlayerDashCooldown = 0.0f;
	g_PlayerDashElapsed = PlayerConstants::Dash::Normal.Duration;
	g_PlayerDashEffectTime = 0.0f;
	g_PlayerDashStartPos = g_PlayerPos;
	g_PlayerDashEndPos = g_PlayerPos;
	g_PlayerDashDirection = { 0.0f, 0.0f };
	g_PlayerDashRequested = false;
	g_PlayerDashActive = false;
	g_PlayerDashIsEmpowered = false;
	g_PlayerDashEffectIsEmpowered = false;
	g_PlayerIsSprinting = false;
	g_PlayerSprintDustElapsed = 0.0f;
	g_PlayerEmpoweredDashModeActive = false;
	g_PlayerEmpoweredDashCharges = 0;
	g_PlayerEmpoweredDashAttackPending = false;
	for (PlayerSlashTrail& trail : g_PlayerSlashTrails)
	{
		trail = {};
	}
	g_PlayerSlashTrailReplaceIndex = 0;
}

static float GetDashDistanceRatio(float progress)
{
	progress = Saturate(progress);
	const float remaining = 1.0f - progress;
	return 1.0f - remaining * remaining;
}

static void UpdateSlashTrails(float delta_time)
{
	for (PlayerSlashTrail& trail : g_PlayerSlashTrails)
	{
		if (!trail.Active)
		{
			continue;
		}
		trail.Elapsed += std::max(delta_time, 0.0f);
		if (trail.Elapsed >= PlayerConstants::Slash::TrailLifetime)
		{
			trail.Active = false;
		}
	}
}

static void SpawnSlashTrail(const XMFLOAT2& start, const XMFLOAT2& end)
{
	PlayerSlashTrail* target = nullptr;
	for (PlayerSlashTrail& trail : g_PlayerSlashTrails)
	{
		if (!trail.Active)
		{
			target = &trail;
			break;
		}
	}
	if (!target)
	{
		target = &g_PlayerSlashTrails[g_PlayerSlashTrailReplaceIndex];
		g_PlayerSlashTrailReplaceIndex = (g_PlayerSlashTrailReplaceIndex + 1) % PlayerConstants::Slash::TrailMax;
	}

	target->Start = start;
	target->End = end;
	target->Elapsed = 0.0f;
	target->Active = true;
}

static void DrawSlashTrails()
{
	if (g_PlayerSlashArcTextureID == TEXTURE_INVALID_ID || g_PlayerSlashChainTextureID == TEXTURE_INVALID_ID ||
	    g_PlayerSlashFinishTextureID == TEXTURE_INVALID_ID || g_PlayerSlashLaserTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	for (const PlayerSlashTrail& trail : g_PlayerSlashTrails)
	{
		if (!trail.Active)
		{
			continue;
		}

		const XMFLOAT2 delta = {
			trail.End.x - trail.Start.x,
			trail.End.y - trail.Start.y,
		};
		const float length = Length(delta);
		if (length <= 1.0f)
		{
			continue;
		}

		const float slash_elapsed = std::max(trail.Elapsed - PlayerConstants::Slash::LaserDrawDuration, 0.0f);
		const float slash_duration = PlayerConstants::Slash::TrailLifetime - PlayerConstants::Slash::LaserDrawDuration;
		const float progress = Saturate(slash_elapsed / slash_duration);
		const float fade = progress < 0.42f ? 1.0f : (1.0f - progress) / 0.58f;
		const float rotation = std::atan2(delta.y, delta.x);
		const XMFLOAT2 center = {
			(trail.Start.x + trail.End.x) * 0.5f,
			(trail.Start.y + trail.End.y) * 0.5f,
		};

		// 먼저 빠르게 뻗는 시간 레이저로 절단선을 그린다.
		// 절단선이 완성된 뒤에 베기 애니메이션을 시작한다.
		const float laser_fade =
		    Saturate(1.0f - std::max(trail.Elapsed - PlayerConstants::Slash::LaserDrawDuration, 0.0f) /
		                        PlayerConstants::Slash::LaserFadeDuration);
		if (laser_fade > 0.0f)
		{
			const float laser_linear = Saturate(trail.Elapsed / PlayerConstants::Slash::LaserDrawDuration);
			const float laser_reveal = 1.0f - (1.0f - laser_linear) * (1.0f - laser_linear) * (1.0f - laser_linear);
			const float laser_length = length * laser_reveal;
			const XMFLOAT2 laser_center = {
				trail.Start.x + delta.x * laser_reveal * 0.5f,
				trail.Start.y + delta.y * laser_reveal * 0.5f,
			};
			const XMFLOAT2 laser_tip = {
				trail.Start.x + delta.x * laser_reveal,
				trail.Start.y + delta.y * laser_reveal,
			};
			Sprite_DrawRegionRotated(g_PlayerSlashLaserTextureID, {
				.Position = laser_center,
				.Size = { laser_length, 22.0f },
				.Rotation = rotation,
				.Source = { 0, 0, 8, 8 },
				.Color = { 0.34f, 0.08f, 1.0f, laser_fade * 0.34f },
				.Additive = true,
			});
			Sprite_DrawRegionRotated(g_PlayerSlashLaserTextureID, {
				.Position = laser_center,
				.Size = { laser_length, 8.0f },
				.Rotation = rotation,
				.Source = { 0, 0, 8, 8 },
				.Color = { 0.08f, 0.88f, 1.0f, laser_fade * 0.88f },
				.Additive = true,
			});
			Sprite_DrawRegionRotated(g_PlayerSlashLaserTextureID, {
				.Position = laser_center,
				.Size = { laser_length, 2.5f },
				.Rotation = rotation,
				.Source = { 0, 0, 8, 8 },
				.Color = { 1.0f, 1.0f, 1.0f, laser_fade },
				.Additive = true,
			});
			if (laser_linear < 1.0f)
			{
				Sprite_DrawRegionRotated(g_PlayerSlashLaserTextureID, {
					.Position = laser_tip,
					.Size = { 54.0f, 5.0f },
					.Rotation = rotation,
					.Source = { 0, 0, 8, 8 },
					.Color = { 0.8f, 1.0f, 1.0f, 0.9f },
					.Additive = true,
				});
				Sprite_DrawRegionRotated(g_PlayerSlashLaserTextureID, {
					.Position = laser_tip,
					.Size = { 34.0f, 4.0f },
					.Rotation = rotation + XM_PIDIV2,
					.Source = { 0, 0, 8, 8 },
					.Color = { 0.68f, 0.86f, 1.0f, 0.84f },
					.Additive = true,
				});
			}
		}

		if (trail.Elapsed < PlayerConstants::Slash::LaserDrawDuration)
		{
			continue;
		}

		const int main_frame = std::min(static_cast<int>(slash_elapsed / PlayerConstants::Slash::FrameTime),
		                                PlayerConstants::Slash::ArcFrameCount - 1);
		const int main_frame_x = main_frame * PlayerConstants::Slash::FrameSize;
		const int main_frame_y = 0;

		// Frostwindz Slash 3으로 넓은 잔상을 먼저 펼치고 Slash 1으로
		// 대시 방향을 따라 칼날 윤곽을 그린다.
		Sprite_DrawRegionRotated(g_PlayerSlashFinishTextureID, {
			.Position = center,
			.Size = { length + 112.0f, 224.0f },
			.Rotation = rotation,
			.Source = { main_frame_x, main_frame_y, PlayerConstants::Slash::FrameSize, PlayerConstants::Slash::FrameSize },
			.Color = { 1.0f, 1.0f, 1.0f, fade * 0.72f },
			.Additive = true,
		});
		Sprite_DrawRegionRotated(g_PlayerSlashArcTextureID, {
			.Position = center,
			.Size = { length + 68.0f, 184.0f },
			.Rotation = rotation,
			.Source = { main_frame_x, main_frame_y, PlayerConstants::Slash::FrameSize, PlayerConstants::Slash::FrameSize },
			.Color = { 1.0f, 1.0f, 1.0f, fade },
			.Additive = true,
		});

		// 피해 판정 다섯 번에 맞춰 짧은 후속 베기를 다섯 번 그린다.
		// 대시 경로를 따라 각도를 번갈아 바꿔 연속 베기를 표현한다.
		const XMFLOAT2 direction = NormalizeOr(delta, { 1.0f, 0.0f });
		const XMFLOAT2 normal = { -direction.y, direction.x };
		for (int i = 0; i < PlayerConstants::Slash::CutCount; ++i)
		{
			const float reveal_time = i * PlayerConstants::Slash::CutInterval;
			const float cut_age = slash_elapsed - reveal_time;
			if (cut_age < 0.0f || cut_age >= PlayerConstants::Slash::CutVisibleDuration)
			{
				continue;
			}
			const float amount = (static_cast<float>(i) + 0.5f) / static_cast<float>(PlayerConstants::Slash::CutCount);
			const float side_sign = i % 2 == 0 ? 1.0f : -1.0f;
			const float side = side_sign * (12.0f + static_cast<float>(i % 3) * 8.0f);
			const XMFLOAT2 position = {
				trail.Start.x + delta.x * amount + normal.x * side,
				trail.Start.y + delta.y * amount + normal.y * side,
			};
			const int cut_frame = std::min(2 + static_cast<int>(cut_age / PlayerConstants::Slash::CutFrameTime),
			                               PlayerConstants::Slash::ChainFrameCount - 1);
			const int cut_frame_x = cut_frame * PlayerConstants::Slash::FrameSize;
			const float cut_fade = Saturate(1.0f - cut_age / PlayerConstants::Slash::CutVisibleDuration);
			const float cut_rotation = rotation + side_sign * 0.62f;
			for (int streak = 0; streak < PlayerConstants::Slash::StreaksPerHit; ++streak)
			{
				const float streak_offset = static_cast<float>(streak - 1);
				const XMFLOAT2 streak_position = {
					position.x + direction.x * streak_offset * 12.0f + normal.x * streak_offset * 9.0f,
					position.y + direction.y * streak_offset * 12.0f + normal.y * streak_offset * 9.0f,
				};
				const float streak_rotation = cut_rotation + streak_offset * 0.09f;
				const float streak_alpha = streak == 1 ? 1.0f : 0.72f;
				Sprite_DrawRegionRotated(streak == 1 ? g_PlayerSlashChainTextureID : g_PlayerSlashArcTextureID, {
					.Position = streak_position,
					.Size = {
						206.0f + static_cast<float>(i % 3) * 22.0f - std::abs(streak_offset) * 28.0f,
						132.0f - std::abs(streak_offset) * 18.0f,
					},
					.Rotation = streak_rotation,
					.Source = { cut_frame_x, 0, PlayerConstants::Slash::FrameSize, PlayerConstants::Slash::FrameSize },
					.Color = { 1.0f, 1.0f, 1.0f, cut_fade * streak_alpha },
					.Additive = true,
				});
			}
		}
	}
}

namespace GamePlayer
{
	void Initialize()
	{
		g_PlayerPos = { 0.0f, 0.0f };
		g_PlayerAimDirection = { 0.0f, -1.0f };
		g_PlayerMaxHitPoint = PlayerConstants::Stats::MaxHitPoint;
		g_PlayerHitPoint = g_PlayerMaxHitPoint;
		g_PlayerSpeed = PlayerConstants::Stats::MoveSpeed;
		g_PlayerDamageInvincibilityTimer = 0.0f;
		g_PlayerDamageFeedback = 0.0f;
		g_PlayerLevel = 1;
		g_PlayerExperience = 0;
		ResetDashState();
		g_PlayerDashAudioID = Audio_Load("asset/sound/pixabay-fast-swoosh-03-229316.wav");
		g_PlayerEmpoweredDashAudioID = Audio_Load("asset/sound/hove-sword-stab-whoosh-01.wav");
		g_PlayerDamageAudioID = Audio_Load("asset/sound/leohpaz-11-human-damage-3.wav");
		g_PlayerLevelUpAudioID = Audio_Load("asset/sound/rpg3-level-up-success-short.wav");
		g_PlayerIdleTextureID = Texture_Load(L"asset/texture/character/player/Rogue/Idle/Idle-Sheet.png");
		g_PlayerRunTextureID = Texture_Load(L"asset/texture/character/player/Rogue/Run/Run-Sheet.png");
		g_PlayerDieTextureID = Texture_Load(L"asset/texture/character/player/Rogue/Death/Death-Sheet.png");
		g_PlayerSlashArcTextureID =
		    Texture_Load(L"asset/texture/vfx/frostwindz/pixel-art-slashes/slash_1_color3_128.png", false);
		g_PlayerSlashChainTextureID =
		    Texture_Load(L"asset/texture/vfx/frostwindz/pixel-art-slashes/slash_2_color3_128.png", false);
		g_PlayerSlashFinishTextureID =
		    Texture_Load(L"asset/texture/vfx/frostwindz/pixel-art-slashes/slash_3_color3_128.png", false);
		g_PlayerSlashLaserTextureID = Texture_Load(L"asset/texture/white_square.png", false);
		g_PlayerCurrentFrame = 0;
		g_PlayerAnimTimer = 0.0f;
		g_PlayerIsMoving = false;
		g_PlayerFacingLeft = false;
		g_PlayerDieCurrentFrame = 0;
		g_PlayerDieAnimTimer = 0.0f;
		g_PlayerDieAnimationVisible = false;
		g_PlayerDieAnimationFinished = false;
	}

	void Finalize()
	{
		if (g_PlayerDashAudioID >= 0)
		{
			Audio_Unload(g_PlayerDashAudioID);
			g_PlayerDashAudioID = -1;
		}
		if (g_PlayerEmpoweredDashAudioID >= 0)
		{
			Audio_Unload(g_PlayerEmpoweredDashAudioID);
			g_PlayerEmpoweredDashAudioID = -1;
		}
		if (g_PlayerDamageAudioID >= 0)
		{
			Audio_Unload(g_PlayerDamageAudioID);
			g_PlayerDamageAudioID = -1;
		}
		if (g_PlayerLevelUpAudioID >= 0)
		{
			Audio_Unload(g_PlayerLevelUpAudioID);
			g_PlayerLevelUpAudioID = -1;
		}
		Texture_Release(g_PlayerIdleTextureID);
		Texture_Release(g_PlayerRunTextureID);
		Texture_Release(g_PlayerDieTextureID);
		Texture_Release(g_PlayerSlashArcTextureID);
		Texture_Release(g_PlayerSlashChainTextureID);
		Texture_Release(g_PlayerSlashFinishTextureID);
		Texture_Release(g_PlayerSlashLaserTextureID);
		g_PlayerIdleTextureID = TEXTURE_INVALID_ID;
		g_PlayerRunTextureID = TEXTURE_INVALID_ID;
		g_PlayerDieTextureID = TEXTURE_INVALID_ID;
		g_PlayerSlashArcTextureID = TEXTURE_INVALID_ID;
		g_PlayerSlashChainTextureID = TEXTURE_INVALID_ID;
		g_PlayerSlashFinishTextureID = TEXTURE_INVALID_ID;
		g_PlayerSlashLaserTextureID = TEXTURE_INVALID_ID;
	}

	void BeginEmpoweredDashMode()
	{
		g_PlayerEmpoweredDashModeActive = true;
		g_PlayerEmpoweredDashCharges = PlayerConstants::Dash::EmpoweredCharges;
	}

	void EndEmpoweredDashMode()
	{
		g_PlayerEmpoweredDashModeActive = false;
		g_PlayerEmpoweredDashCharges = 0;
	}

	void RequestDash()
	{
		g_PlayerDashRequested = true;
	}

	void SetSprinting(bool is_sprinting)
	{
		if (g_PlayerIsSprinting == is_sprinting)
		{
			return;
		}
		g_PlayerIsSprinting = is_sprinting;
		// 빠른 달리기를 시작하면 첫 발자국을 즉시 생성한다.
		g_PlayerSprintDustElapsed = is_sprinting ? PlayerConstants::Movement::SprintDustInterval : 0.0f;
	}

	bool ConsumeEmpoweredDashAttack(XMFLOAT2& out_start, XMFLOAT2& out_end)
	{
		if (!g_PlayerEmpoweredDashAttackPending)
		{
			return false;
		}
		out_start = g_PlayerEmpoweredDashAttackStart;
		out_end = g_PlayerEmpoweredDashAttackEnd;
		g_PlayerEmpoweredDashAttackPending = false;
		return true;
	}

	void UpdateEmpoweredDashAttack(float delta_time)
	{

		XMFLOAT2 slash_start{};
		XMFLOAT2 slash_end{};
		if (ConsumeEmpoweredDashAttack(slash_start, slash_end))
		{
			GameEnemy::ApplyDashSlashDamage(slash_start, slash_end, PlayerConstants::Slash::SlashHalfWidth,
			                                PlayerConstants::Slash::DamagePerHit);
		}
		GameEnemy::UpdateDashSlashAttacks(delta_time);
	}

	bool IsEmpoweredDashModeActive()
	{
		return g_PlayerEmpoweredDashModeActive && g_PlayerEmpoweredDashCharges > 0;
	}

	int GetEmpoweredDashCharges()
	{
		return IsEmpoweredDashModeActive() ? g_PlayerEmpoweredDashCharges : 0;
	}

	void Update(float delta_time)
	{
		delta_time = std::max(delta_time, 0.0f);
		UpdateSlashTrails(delta_time);
		g_PlayerDamageInvincibilityTimer = std::max(0.0f, g_PlayerDamageInvincibilityTimer - delta_time);
		g_PlayerDashCooldown = std::max(0.0f, g_PlayerDashCooldown - delta_time);
		g_PlayerDashEffectTime = std::max(0.0f, g_PlayerDashEffectTime - delta_time);
		if (g_PlayerDashEffectTime <= 0.0f)
		{
			g_PlayerDashEffectIsEmpowered = false;
		}

		XMFLOAT2 move = { 0.0f, 0.0f };

		if (InputKeyboard_IsPress(KK_W) || InputKeyboard_IsPress(KK_UP))
		{
			move.y -= 1.0f;
		}
		if (InputKeyboard_IsPress(KK_S) || InputKeyboard_IsPress(KK_DOWN))
		{
			move.y += 1.0f;
		}
		if (InputKeyboard_IsPress(KK_A) || InputKeyboard_IsPress(KK_LEFT))
		{
			move.x -= 1.0f;
		}
		if (InputKeyboard_IsPress(KK_D) || InputKeyboard_IsPress(KK_RIGHT))
		{
			move.x += 1.0f;
		}

		const bool has_move_input = move.x != 0.0f || move.y != 0.0f;
		if (has_move_input)
		{
			XMStoreFloat2(&move, XMVector2Normalize(XMLoadFloat2(&move)));
		}

		if (g_PlayerIsMoving != has_move_input)
		{
			g_PlayerCurrentFrame = 0;
			g_PlayerAnimTimer = 0.0f;
		}
		g_PlayerIsMoving = has_move_input;
		if (g_PlayerIsSprinting && has_move_input)
		{
			g_PlayerSprintDustElapsed += delta_time;
			while (g_PlayerSprintDustElapsed >= PlayerConstants::Movement::SprintDustInterval)
			{
				g_PlayerSprintDustElapsed -= PlayerConstants::Movement::SprintDustInterval;
				cGameEffectManager::GetInstance().Play(
				    GameEffectType::SprintDust,
				    { g_PlayerPos.x, g_PlayerPos.y + PlayerConstants::Movement::FeetYOffset }, 0.92f,
				    { 0.90f, 0.86f, 0.76f, 1.0f });
			}
		}
		else if (!has_move_input)
		{
			g_PlayerSprintDustElapsed = PlayerConstants::Movement::SprintDustInterval;
		}
		if (has_move_input && move.x != 0.0f)
		{
			g_PlayerFacingLeft = move.x < 0.0f;
		}
		const int animation_frame_count =
		    g_PlayerIsMoving ? PlayerConstants::Animation::Run.FrameCount : PlayerConstants::Animation::Idle.FrameCount;
		const float animation_frame_duration = g_PlayerIsMoving ? PlayerConstants::Animation::Run.FrameDuration
		                                                        : PlayerConstants::Animation::Idle.FrameDuration;
		g_PlayerAnimTimer += delta_time;
		while (g_PlayerAnimTimer >= animation_frame_duration)
		{
			g_PlayerAnimTimer -= animation_frame_duration;
			g_PlayerCurrentFrame = (g_PlayerCurrentFrame + 1) % animation_frame_count;
		}

		const bool dash_requested = g_PlayerDashRequested;
		g_PlayerDashRequested = false;
		const bool can_aim_dash = g_PlayerEmpoweredDashModeActive && g_PlayerEmpoweredDashCharges > 0;
		if (dash_requested && g_PlayerDashCooldown <= 0.0f && !g_PlayerDashActive && (has_move_input || can_aim_dash))
		{
			const XMFLOAT2 dash_direction = has_move_input ? move : g_PlayerAimDirection;
			g_PlayerDashStartPos = g_PlayerPos;
			g_PlayerDashEndPos = g_PlayerPos;
			g_PlayerDashDirection = dash_direction;
			g_PlayerDashElapsed = 0.0f;
			g_PlayerDashActive = true;
			const int dash_audio_id = can_aim_dash ? g_PlayerEmpoweredDashAudioID : g_PlayerDashAudioID;
			if (dash_audio_id >= 0)
			{
				Audio_Play(dash_audio_id);
			}
			g_PlayerDashIsEmpowered = can_aim_dash;
			g_PlayerDashEffectIsEmpowered = g_PlayerDashIsEmpowered;
			const auto& dash =
			    g_PlayerDashIsEmpowered ? PlayerConstants::Dash::Empowered : PlayerConstants::Dash::Normal;
			g_PlayerDashCooldown = dash.Cooldown;
			g_PlayerDashEffectTime = dash.EffectDuration;
			g_PlayerAimDirection = dash_direction;
			if (g_PlayerDashIsEmpowered)
			{
				--g_PlayerEmpoweredDashCharges;
				if (g_PlayerEmpoweredDashCharges <= 0)
				{
					g_PlayerEmpoweredDashCharges = 0;
					g_PlayerEmpoweredDashModeActive = false;
				}
			}
		}

		if (g_PlayerDashActive)
		{
			const auto& dash =
			    g_PlayerDashIsEmpowered ? PlayerConstants::Dash::Empowered : PlayerConstants::Dash::Normal;
			const float previous_progress = g_PlayerDashElapsed / dash.Duration;
			g_PlayerDashElapsed = std::min(g_PlayerDashElapsed + delta_time, dash.Duration);
			const float current_progress = g_PlayerDashElapsed / dash.Duration;
			const float distance =
			    dash.Distance * (GetDashDistanceRatio(current_progress) - GetDashDistanceRatio(previous_progress));
			const XMFLOAT2 dash_movement = {
				g_PlayerDashDirection.x * distance,
				g_PlayerDashDirection.y * distance,
			};
			g_PlayerPos =
			    ProceduralMap_MoveActorCircle(g_PlayerPos, dash_movement, PlayerConstants::Movement::CollisionRadius);
			g_PlayerDashEndPos = g_PlayerPos;
			if (g_PlayerDashElapsed >= dash.Duration)
			{
				g_PlayerDashActive = false;
				g_PlayerDashElapsed = PlayerConstants::Dash::Normal.Duration;
				if (g_PlayerDashIsEmpowered)
				{
					g_PlayerEmpoweredDashAttackStart = g_PlayerDashStartPos;
					g_PlayerEmpoweredDashAttackEnd = g_PlayerDashEndPos;
					g_PlayerEmpoweredDashAttackPending = true;
					SpawnSlashTrail(g_PlayerDashStartPos, g_PlayerDashEndPos);
				}
				g_PlayerDashIsEmpowered = false;
			}
			return;
		}

		if (has_move_input)
		{
			const float move_speed =
			    g_PlayerSpeed * (g_PlayerIsSprinting ? PlayerConstants::Movement::SprintSpeedMultiplier : 1.0f);
			const XMFLOAT2 movement = {
				move.x * move_speed * delta_time,
				move.y * move_speed * delta_time,
			};
			g_PlayerPos =
			    ProceduralMap_MoveActorCircle(g_PlayerPos, movement, PlayerConstants::Movement::CollisionRadius);
			g_PlayerAimDirection = move;
		}
	}

	void SetPosition(const XMFLOAT2& position)
	{
		g_PlayerPos = position;
		ResetDashState();
	}

	void SetAimTarget(const XMFLOAT2& world_position)
	{
		const XMFLOAT2 aim = {
			world_position.x - g_PlayerPos.x,
			world_position.y - g_PlayerPos.y,
		};
		const float aim_length_sq = LengthSquared(aim);
		if (aim_length_sq <= PlayerConstants::Movement::AimDeadZoneSq)
		{
			return;
		}
		g_PlayerAimDirection = NormalizeOr(aim, g_PlayerAimDirection);
	}

	void ApplyDamage(float damage)
	{
		if (damage <= 0.0f || g_PlayerHitPoint <= 0.0f || g_PlayerDamageInvincibilityTimer > 0.0f || g_PlayerDashActive)
		{
			return;
		}

		g_PlayerHitPoint = std::clamp(g_PlayerHitPoint - damage, 0.0f, g_PlayerMaxHitPoint);
		g_PlayerDamageInvincibilityTimer = PlayerConstants::Stats::DamageInvincibleDuration;
		g_PlayerDamageFeedback = std::max(g_PlayerDamageFeedback, damage);
		if (g_PlayerDamageAudioID >= 0)
		{
			Audio_Play(g_PlayerDamageAudioID);
		}
		GameDamageText::Spawn(damage, g_PlayerPos, { 1.0f, 0.12f, 0.08f, 1.0f });
		Blood::Spawn(g_PlayerPos);
	}

	bool ConsumeDamageFeedback(float& out_damage)
	{
		if (g_PlayerDamageFeedback <= 0.0f)
		{
			return false;
		}

		out_damage = g_PlayerDamageFeedback;
		g_PlayerDamageFeedback = 0.0f;
		return true;
	}

	float Heal(float amount)
	{
		if (amount <= 0.0f || g_PlayerHitPoint <= 0.0f)
		{
			return 0.0f;
		}

		const float previous_hit_point = g_PlayerHitPoint;
		g_PlayerHitPoint = std::min(g_PlayerHitPoint + amount, g_PlayerMaxHitPoint);
		const float healed_amount = g_PlayerHitPoint - previous_hit_point;
		if (healed_amount > 0.0f)
		{
			GameDamageText::SpawnHealing(healed_amount, g_PlayerPos);
		}
		return healed_amount;
	}

	void AddExperience(int experience)
	{
		if (experience <= 0)
		{
			return;
		}

		g_PlayerExperience += experience;
		const int previous_level = g_PlayerLevel;
		int required_experience = GetRequiredExperience(g_PlayerLevel);
		while (g_PlayerExperience >= required_experience)
		{
			g_PlayerExperience -= required_experience;
			++g_PlayerLevel;
			required_experience = GetRequiredExperience(g_PlayerLevel);
		}
		if (g_PlayerLevel > previous_level && g_PlayerLevelUpAudioID >= 0)
		{
			Audio_Play(g_PlayerLevelUpAudioID);
		}
	}

	void IncreaseMaxHitPoint(float amount)
	{
		if (amount <= 0.0f)
		{
			return;
		}

		g_PlayerMaxHitPoint += amount;
		g_PlayerHitPoint = std::min(g_PlayerHitPoint + amount, g_PlayerMaxHitPoint);
	}

	void MultiplyMoveSpeed(float multiplier)
	{
		if (multiplier > 0.0f)
		{
			g_PlayerSpeed *= multiplier;
		}
	}

	float GetHitPoint()
	{
		return g_PlayerHitPoint;
	}

	float GetMaxHitPoint()
	{
		return g_PlayerMaxHitPoint;
	}

	int GetLevel()
	{
		return g_PlayerLevel;
	}

	int GetExperience()
	{
		return g_PlayerExperience;
	}

	int GetExperienceToNextLevel()
	{
		return GetRequiredExperience(g_PlayerLevel);
	}

	void PrepareDeathSequence()
	{
		g_PlayerDamageInvincibilityTimer = 0.0f;
		g_PlayerDashEffectTime = 0.0f;
		g_PlayerDashRequested = false;
		g_PlayerDashActive = false;
		g_PlayerDashIsEmpowered = false;
		g_PlayerDashEffectIsEmpowered = false;
		g_PlayerEmpoweredDashModeActive = false;
		g_PlayerEmpoweredDashCharges = 0;
		g_PlayerEmpoweredDashAttackPending = false;
		g_PlayerIsMoving = false;
		g_PlayerCurrentFrame = 0;
		g_PlayerDieCurrentFrame = 0;
		g_PlayerDieAnimTimer = 0.0f;
		g_PlayerDieAnimationVisible = false;
		g_PlayerDieAnimationFinished = false;
	}

	void BeginDeathAnimation()
	{
		g_PlayerDieCurrentFrame = 0;
		g_PlayerDieAnimTimer = 0.0f;
		g_PlayerDieAnimationVisible = true;
		g_PlayerDieAnimationFinished = false;
	}

	void UpdateDeathAnimation(float delta_time)
	{
		if (!g_PlayerDieAnimationVisible || g_PlayerDieAnimationFinished)
		{
			return;
		}

		g_PlayerDieAnimTimer += std::max(delta_time, 0.0f);
		g_PlayerDieCurrentFrame =
		    std::min(static_cast<int>(g_PlayerDieAnimTimer / PlayerConstants::Animation::Die.FrameDuration),
		             PlayerConstants::Animation::Die.FrameCount - 1);
		if (g_PlayerDieAnimTimer >=
		    PlayerConstants::Animation::Die.FrameCount * PlayerConstants::Animation::Die.FrameDuration)
		{
			g_PlayerDieCurrentFrame = PlayerConstants::Animation::Die.FrameCount - 1;
			g_PlayerDieAnimationFinished = true;
		}
	}

	bool IsDeathAnimationFinished()
	{
		return g_PlayerDieAnimationFinished;
	}

	void RegisterCollider()
	{
		CollisionSystem_RegisterCircle(0, CollisionLayer::Player, CollisionLayer::Enemy | CollisionLayer::EnemyBullet,
		                               g_PlayerPos, PlayerConstants::Movement::CollisionRadius,
		                               g_PlayerHitPoint > 0.0f);
	}

	void HandleCollisionHits()
	{
		for (int i = 0; i < CollisionSystem_GetHitCount(); ++i)
		{
			const cCollisionHit* hit = CollisionSystem_GetHit(i);
			if (!hit)
			{
				continue;
			}

			const bool enemy_hit_player =
			    (hit->BodyA.Layer == CollisionLayer::Enemy && hit->BodyB.Layer == CollisionLayer::Player) ||
			    (hit->BodyA.Layer == CollisionLayer::Player && hit->BodyB.Layer == CollisionLayer::Enemy);
			if (enemy_hit_player)
			{
				// 접근 데미지
				ApplyDamage(PlayerConstants::Stats::ContactDamage);
				return;
			}

			int enemy_bullet_id = COLLISION_INVALID_ID;
			if (hit->BodyA.Layer == CollisionLayer::EnemyBullet && hit->BodyB.Layer == CollisionLayer::Player)
			{
				enemy_bullet_id = hit->BodyA.OwnerID;
			}
			else if (hit->BodyA.Layer == CollisionLayer::Player && hit->BodyB.Layer == CollisionLayer::EnemyBullet)
			{
				enemy_bullet_id = hit->BodyB.OwnerID;
			}
			if (enemy_bullet_id != COLLISION_INVALID_ID)
			{
				float damage = 0.0f;
				if (GameEnemy::ConsumeEnemyBullet(enemy_bullet_id, damage))
				{
					ApplyDamage(damage);
					return;
				}
			}
		}
	}

	void Draw()
	{
		if (!g_PlayerDieAnimationVisible)
		{
			const bool lighting_was_enabled = Sprite_SetLightingEnabled(false);
			DrawSlashTrails();
			Sprite_SetLightingEnabled(lighting_was_enabled);
		}
		const int texture_id = g_PlayerDieAnimationVisible
		                           ? g_PlayerDieTextureID
		                           : (g_PlayerIsMoving ? g_PlayerRunTextureID : g_PlayerIdleTextureID);
		const int frame_width = g_PlayerDieAnimationVisible
		                            ? PlayerConstants::Animation::Die.FrameWidth
		                            : (g_PlayerIsMoving ? PlayerConstants::Animation::Run.FrameWidth
		                                                : PlayerConstants::Animation::Idle.FrameWidth);
		const int frame_height = g_PlayerDieAnimationVisible
		                             ? PlayerConstants::Animation::Die.FrameHeight
		                             : (g_PlayerIsMoving ? PlayerConstants::Animation::Run.FrameHeight
		                                                 : PlayerConstants::Animation::Idle.FrameHeight);
		const int current_frame = g_PlayerDieAnimationVisible ? g_PlayerDieCurrentFrame : g_PlayerCurrentFrame;
		const int frame_x = current_frame * frame_width;
		const float unsigned_draw_width = frame_width * PlayerConstants::Animation::SpriteScale;
		const float draw_width = g_PlayerFacingLeft ? -unsigned_draw_width : unsigned_draw_width;
		const float draw_height = frame_height * PlayerConstants::Animation::SpriteScale;
		const float draw_y =
		    g_PlayerPos.y +
		    (g_PlayerIsMoving && !g_PlayerDieAnimationVisible ? PlayerConstants::Animation::RunDrawYOffset : 0.0f);
		if (g_PlayerDieAnimationVisible)
		{
			Sprite_DrawRegion(texture_id, { g_PlayerPos.x, draw_y }, { draw_width, draw_height },
			                  { frame_x, 0, frame_width, frame_height }, { 1.0f, 1.0f, 1.0f, 1.0f });
			return;
		}
		const bool damage_flash = g_PlayerDamageInvincibilityTimer > PlayerConstants::Stats::DamageInvincibleDuration -
		                                                                 PlayerConstants::Stats::DamageFlashDuration;
		const XMFLOAT4 player_color =
		    damage_flash ? XMFLOAT4{ 1.0f, 0.15f, 0.15f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };

		if (g_PlayerDashEffectTime > 0.0f)
		{
			const float effect_duration = g_PlayerDashEffectIsEmpowered
			                                  ? PlayerConstants::Dash::Empowered.EffectDuration
			                                  : PlayerConstants::Dash::Normal.EffectDuration;
			const float fade = Saturate(g_PlayerDashEffectTime / effect_duration);
			const bool lighting_was_enabled = Sprite_SetLightingEnabled(false);
			for (int i = 0; i < PlayerConstants::Dash::AfterimageCount; ++i)
			{
				const float color_amount =
				    static_cast<float>(i) / static_cast<float>(PlayerConstants::Dash::AfterimageCount - 1);
				const float path_amount =
				    static_cast<float>(i) / static_cast<float>(PlayerConstants::Dash::AfterimageCount);
				const XMFLOAT2 afterimage_position = {
					g_PlayerDashStartPos.x + (g_PlayerDashEndPos.x - g_PlayerDashStartPos.x) * path_amount,
					g_PlayerDashStartPos.y + (g_PlayerDashEndPos.y - g_PlayerDashStartPos.y) * path_amount,
				};
				// 오래된 잔상은 보라색으로, 플레이어에 가까운 잔상은 밝은 파란색으로 바꾼다.
				// 알파를 높여 어두운 바닥과 밝은 전투 이펙트 위에서도
				// 대시 잔상이 잘 보이도록 한다.
				const XMFLOAT4 afterimage_color = {
					0.72f - 0.46f * color_amount,
					0.30f + 0.42f * color_amount,
					1.0f,
					fade * (0.24f + 0.30f * color_amount),
				};
				Sprite_DrawRegion(
				    texture_id,
				    { afterimage_position.x, afterimage_position.y + PlayerConstants::Animation::RunDrawYOffset },
				    { draw_width, draw_height }, { frame_x, 0, frame_width, frame_height }, afterimage_color);
			}
			Sprite_SetLightingEnabled(lighting_was_enabled);
			Sprite_DrawRegion(texture_id, { g_PlayerPos.x, draw_y }, { draw_width, draw_height },
			                  { frame_x, 0, frame_width, frame_height },
			                  damage_flash ? player_color : XMFLOAT4{ 0.58f, 0.76f, 1.0f, 1.0f });
			return;
		}

		Sprite_DrawRegion(texture_id, { g_PlayerPos.x, draw_y }, { draw_width, draw_height },
		                  { frame_x, 0, frame_width, frame_height }, player_color);
	}

	void DrawMapMarker(const XMFLOAT2& map_origin, float world_scale, bool expanded)
	{
		const XMFLOAT2 marker_position = {
			map_origin.x + g_PlayerPos.x * world_scale,
			map_origin.y + g_PlayerPos.y * world_scale,
		};
		const float marker_size = expanded ? 12.0f : 8.0f;
		Sprite_DrawSized(g_PlayerSlashLaserTextureID, marker_position.x, marker_position.y, marker_size, marker_size,
		                 { 0.10f, 1.0f, 0.20f, 1.0f });
	}

	XMFLOAT2 GetAimDirection()
	{
		return g_PlayerAimDirection;
	}

	XMFLOAT2 GetPosition()
	{
		return g_PlayerPos;
	}
} // namespace GamePlayer
