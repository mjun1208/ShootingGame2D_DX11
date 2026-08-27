#include "game_player.h"

#include "Audio.h"
#include "blood.h"
#include "collision.h"
#include "direct3d.h"
#include "game_data_manager.h"
#include "game_damage_text.h"
#include "game_effect.h"
#include "game_enemy.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include "input_keyboard.h"
#include "procedural_map.h"
#include <array>
#include <algorithm>
#include <cmath>

#include <DirectXMath.h>
using namespace DirectX;

static int g_PlayerIdleTextureID = TEXTURE_INVALID_ID;
static int g_PlayerRunTextureID = TEXTURE_INVALID_ID;
static int g_PlayerDieTextureID = TEXTURE_INVALID_ID;

static XMFLOAT2 g_PlayerPos;
static XMFLOAT2 g_PlayerAimDirection = { 0.0f, -1.0f };


namespace
{
	namespace PlayerStats
	{
		constexpr float MaxHitPoint = 100.0f;
		constexpr float MoveSpeed = 500.0f;
		constexpr float ContactDamage = 10.0f;
		constexpr float DamageInvincibleDuration = 0.65f;
		constexpr float DamageFlashDuration = 0.12f;
	}

	namespace PlayerDamage
	{
		constexpr const char* SoundPath =
			"asset/sound/leohpaz-11-human-damage-3.wav";
	}

	namespace PlayerLevelUp
	{
		constexpr const char* SoundPath =
			"asset/sound/rpg3-level-up-success-short.wav";
	}

	namespace PlayerAnimation
	{
		struct Description
		{
			int FrameCount;
			int FrameWidth;
			int FrameHeight;
			float FrameDuration;
		};

		constexpr float SpriteScale = 3.0f;
		constexpr Description Idle{ 4, 32, 32, 0.14f };
		constexpr Description Run{ 6, 64, 64, 0.08f };
		constexpr Description Die{ 6, 64, 32, 0.12f };
		constexpr float RunDrawYOffset = -16.0f * SpriteScale;
	}

	namespace PlayerMovement
	{
		constexpr float AimDeadZoneSq = 16.0f;
		constexpr float CollisionRadius = 30.0f;
		constexpr float FeetYOffset = 48.0f;
		constexpr float SprintSpeedMultiplier = 1.65f;
		constexpr float SprintDustInterval = 0.10f;
	}

	namespace PlayerDash
	{
		constexpr const char* SoundPath =
			"asset/sound/pixabay-fast-swoosh-03-229316.wav";
		constexpr const char* EmpoweredSoundPath =
			"asset/sound/hove-sword-stab-whoosh-01.wav";
		constexpr float Distance = 240.0f;
		constexpr float Duration = 0.18f;
		constexpr float Cooldown = 0.8f;
		constexpr float EffectDuration = 0.22f;
		constexpr int AfterimageCount = 6;
		constexpr int EmpoweredCharges = 3;
		constexpr float EmpoweredDistance = 560.0f;
		constexpr float EmpoweredDuration = 0.12f;
		constexpr float EmpoweredCooldown = EmpoweredDuration;
		constexpr float EmpoweredEffectDuration = 0.48f;
	}

	namespace PlayerSlash
	{
		constexpr float TrailLifetime = 0.72f;
		constexpr float LaserDrawDuration = 0.065f;
		constexpr float LaserFadeDuration = 0.34f;
		constexpr float CutInterval = 0.055f;
		constexpr int CutCount = 5;
		constexpr int TrailMax = 6;
		constexpr int FrameSize = 128;
		constexpr int ArcFrameCount = 9;
		constexpr int ChainFrameCount = 7;
		constexpr float FrameTime = 0.028f;
		constexpr float CutFrameTime = 0.022f;
		constexpr float CutVisibleDuration = 0.18f;
		constexpr int StreaksPerHit = 3;
	}
}

static float g_PlayerHitPoint = PlayerStats::MaxHitPoint;
static float g_PlayerMaxHitPoint = PlayerStats::MaxHitPoint;
static float g_PlayerDamageInvincibilityTimer = 0.0f;
static int g_PlayerLevel = 1;
static int g_PlayerExperience = 0;

static float g_PlayerSpeed = PlayerStats::MoveSpeed;

static int g_PlayerCurrentFrame = 0;
static float g_PlayerAnimTimer = 0.0f;
static bool g_PlayerIsMoving = false;
static bool g_PlayerFacingLeft = false;
static int g_PlayerDieCurrentFrame = 0;
static float g_PlayerDieAnimTimer = 0.0f;
static bool g_PlayerDieAnimationVisible = false;
static bool g_PlayerDieAnimationFinished = false;
static float g_PlayerDashCooldown = 0.0f;
static float g_PlayerDashElapsed = PlayerDash::Duration;
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

static std::array<PlayerSlashTrail, PlayerSlash::TrailMax> g_PlayerSlashTrails{};
static int g_PlayerSlashTrailReplaceIndex = 0;

static int GetRequiredExperience(int level)
{
	return GameDataManager::GetInstance()
		.GetPlayerLevelGameData()
		.GetExperienceToNextLevel(level);
}

static void ResetDashState()
{
	g_PlayerDashCooldown = 0.0f;
	g_PlayerDashElapsed = PlayerDash::Duration;
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
	progress = std::clamp(progress, 0.0f, 1.0f);
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
		if (trail.Elapsed >= PlayerSlash::TrailLifetime)
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
		g_PlayerSlashTrailReplaceIndex =
			(g_PlayerSlashTrailReplaceIndex + 1) % PlayerSlash::TrailMax;
	}

	target->Start = start;
	target->End = end;
	target->Elapsed = 0.0f;
	target->Active = true;
}

static void DrawSlashTrails()
{
	if (g_PlayerSlashArcTextureID == TEXTURE_INVALID_ID ||
		g_PlayerSlashChainTextureID == TEXTURE_INVALID_ID ||
		g_PlayerSlashFinishTextureID == TEXTURE_INVALID_ID ||
		g_PlayerSlashLaserTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	for (const PlayerSlashTrail& trail : g_PlayerSlashTrails)
	{
		if (!trail.Active)
		{
			continue;
		}

		const float delta_x = trail.End.x - trail.Start.x;
		const float delta_y = trail.End.y - trail.Start.y;
		const float length = std::sqrt(delta_x * delta_x + delta_y * delta_y);
		if (length <= 1.0f)
		{
			continue;
		}

		const float slash_elapsed = std::max(
			trail.Elapsed - PlayerSlash::LaserDrawDuration, 0.0f);
		const float slash_duration =
			PlayerSlash::TrailLifetime - PlayerSlash::LaserDrawDuration;
		const float progress = std::clamp(
			slash_elapsed / slash_duration, 0.0f, 1.0f);
		const float fade = progress < 0.42f ? 1.0f :
			(1.0f - progress) / 0.58f;
		const float rotation = std::atan2(delta_y, delta_x);
		const XMFLOAT2 center = {
			(trail.Start.x + trail.End.x) * 0.5f,
			(trail.Start.y + trail.End.y) * 0.5f,
		};

		// First, draw the cut itself as a rapidly extending time-laser. The
		// animated slashes are deliberately delayed until this line is complete.
		const float laser_linear = std::clamp(
			trail.Elapsed / PlayerSlash::LaserDrawDuration, 0.0f, 1.0f);
		const float laser_reveal = 1.0f -
			(1.0f - laser_linear) * (1.0f - laser_linear) * (1.0f - laser_linear);
		const float laser_length = length * laser_reveal;
		const XMFLOAT2 laser_center = {
			trail.Start.x + delta_x * laser_reveal * 0.5f,
			trail.Start.y + delta_y * laser_reveal * 0.5f,
		};
		const XMFLOAT2 laser_tip = {
			trail.Start.x + delta_x * laser_reveal,
			trail.Start.y + delta_y * laser_reveal,
		};
		const float laser_fade = std::clamp(
			1.0f - std::max(
				trail.Elapsed - PlayerSlash::LaserDrawDuration, 0.0f) /
				PlayerSlash::LaserFadeDuration,
			0.0f, 1.0f);
		Sprite_DrawRegionRotated(
			g_PlayerSlashLaserTextureID,
			laser_center.x, laser_center.y,
			laser_length, 22.0f,
			rotation,
			0, 0, 8, 8,
			{ 0.34f, 0.08f, 1.0f, laser_fade * 0.34f },
			true,
			false);
		Sprite_DrawRegionRotated(
			g_PlayerSlashLaserTextureID,
			laser_center.x, laser_center.y,
			laser_length, 8.0f,
			rotation,
			0, 0, 8, 8,
			{ 0.08f, 0.88f, 1.0f, laser_fade * 0.88f },
			true,
			false);
		Sprite_DrawRegionRotated(
			g_PlayerSlashLaserTextureID,
			laser_center.x, laser_center.y,
			laser_length, 2.5f,
			rotation,
			0, 0, 8, 8,
			{ 1.0f, 1.0f, 1.0f, laser_fade },
			true,
			false);
		if (laser_linear < 1.0f)
		{
			Sprite_DrawRegionRotated(
				g_PlayerSlashLaserTextureID,
				laser_tip.x, laser_tip.y,
				54.0f, 5.0f,
				rotation,
				0, 0, 8, 8,
				{ 0.8f, 1.0f, 1.0f, 0.9f },
				true,
				false);
			Sprite_DrawRegionRotated(
				g_PlayerSlashLaserTextureID,
				laser_tip.x, laser_tip.y,
				34.0f, 4.0f,
				rotation + XM_PIDIV2,
				0, 0, 8, 8,
				{ 0.68f, 0.86f, 1.0f, 0.84f },
				true,
				false);
		}

		if (trail.Elapsed < PlayerSlash::LaserDrawDuration)
		{
			continue;
		}

		const int main_frame = std::min(
			static_cast<int>(slash_elapsed / PlayerSlash::FrameTime),
			PlayerSlash::ArcFrameCount - 1);
		const int main_frame_x = main_frame * PlayerSlash::FrameSize;
		const int main_frame_y = 0;

		// Frostwindz Slash 3 blooms first as a broad afterimage, then Slash 1
		// draws the readable blade edge along the dash direction.
		Sprite_DrawRegionRotated(
			g_PlayerSlashFinishTextureID,
			center.x, center.y,
			length + 112.0f, 224.0f,
			rotation,
			main_frame_x, main_frame_y,
			PlayerSlash::FrameSize, PlayerSlash::FrameSize,
			{ 1.0f, 1.0f, 1.0f, fade * 0.72f },
			true,
			false);
		Sprite_DrawRegionRotated(
			g_PlayerSlashArcTextureID,
			center.x, center.y,
			length + 68.0f, 184.0f,
			rotation,
			main_frame_x, main_frame_y,
			PlayerSlash::FrameSize, PlayerSlash::FrameSize,
			{ 1.0f, 1.0f, 1.0f, fade },
			true,
			false);

		// Five compact follow-up cuts match the five damage ticks. Their opposing
		// angles form a rapid blade chain while staying aligned to the dash path.
		const XMFLOAT2 direction = { delta_x / length, delta_y / length };
		const XMFLOAT2 normal = { -direction.y, direction.x };
		for (int i = 0; i < PlayerSlash::CutCount; ++i)
		{
			const float reveal_time = i * PlayerSlash::CutInterval;
			if (slash_elapsed < reveal_time)
			{
				continue;
			}
			const float amount = (static_cast<float>(i) + 0.5f) /
				static_cast<float>(PlayerSlash::CutCount);
			const float side_sign = i % 2 == 0 ? 1.0f : -1.0f;
			const float side = side_sign * (12.0f + static_cast<float>(i % 3) * 8.0f);
			const XMFLOAT2 position = {
				trail.Start.x + delta_x * amount + normal.x * side,
				trail.Start.y + delta_y * amount + normal.y * side,
			};
			const float cut_age = slash_elapsed - reveal_time;
			const int cut_frame = std::min(
				2 + static_cast<int>(cut_age / PlayerSlash::CutFrameTime),
				PlayerSlash::ChainFrameCount - 1);
			const int cut_frame_x = cut_frame * PlayerSlash::FrameSize;
			const float cut_fade = std::clamp(
				1.0f - cut_age / PlayerSlash::CutVisibleDuration,
				0.0f, 1.0f);
			const float cut_rotation = rotation + side_sign * 0.62f;
			for (int streak = 0; streak < PlayerSlash::StreaksPerHit; ++streak)
			{
				const float streak_offset = static_cast<float>(streak - 1);
				const XMFLOAT2 streak_position = {
					position.x + direction.x * streak_offset * 12.0f +
						normal.x * streak_offset * 9.0f,
					position.y + direction.y * streak_offset * 12.0f +
						normal.y * streak_offset * 9.0f,
				};
				const float streak_rotation = cut_rotation + streak_offset * 0.09f;
				const float streak_alpha = streak == 1 ? 1.0f : 0.72f;
				Sprite_DrawRegionRotated(
					streak == 1 ?
						g_PlayerSlashChainTextureID : g_PlayerSlashArcTextureID,
					streak_position.x, streak_position.y,
					206.0f + static_cast<float>(i % 3) * 22.0f -
						std::abs(streak_offset) * 28.0f,
					132.0f - std::abs(streak_offset) * 18.0f,
					streak_rotation,
					cut_frame_x, 0,
					PlayerSlash::FrameSize, PlayerSlash::FrameSize,
					{ 1.0f, 1.0f, 1.0f, cut_fade * streak_alpha },
					true,
					false);
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
		g_PlayerMaxHitPoint = PlayerStats::MaxHitPoint;
		g_PlayerHitPoint = g_PlayerMaxHitPoint;
		g_PlayerSpeed = PlayerStats::MoveSpeed;
		g_PlayerDamageInvincibilityTimer = 0.0f;
		g_PlayerLevel = 1;
		g_PlayerExperience = 0;
		ResetDashState();
		g_PlayerDashAudioID = LoadAudio(PlayerDash::SoundPath);
		g_PlayerEmpoweredDashAudioID = LoadAudio(PlayerDash::EmpoweredSoundPath);
		g_PlayerDamageAudioID = LoadAudio(PlayerDamage::SoundPath);
		g_PlayerLevelUpAudioID = LoadAudio(PlayerLevelUp::SoundPath);
		g_PlayerIdleTextureID = Texture_Load(L"asset/npc/Rogue/Idle/Idle-Sheet.png");
		g_PlayerRunTextureID = Texture_Load(L"asset/npc/Rogue/Run/Run-Sheet.png");
		g_PlayerDieTextureID = Texture_Load(L"asset/npc/Rogue/Death/Death-Sheet.png");
		g_PlayerSlashArcTextureID = Texture_Load(
			L"asset/texture/vfx/frostwindz/pixel-art-slashes/"
			L"slash_1_color3_128.png", false);
		g_PlayerSlashChainTextureID = Texture_Load(
			L"asset/texture/vfx/frostwindz/pixel-art-slashes/"
			L"slash_2_color3_128.png", false);
		g_PlayerSlashFinishTextureID = Texture_Load(
			L"asset/texture/vfx/frostwindz/pixel-art-slashes/"
			L"slash_3_color3_128.png", false);
		g_PlayerSlashLaserTextureID = Texture_Load(
			L"asset/texture/white_square.png", false);
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
			UnloadAudio(g_PlayerDashAudioID);
			g_PlayerDashAudioID = -1;
		}
		if (g_PlayerEmpoweredDashAudioID >= 0)
		{
			UnloadAudio(g_PlayerEmpoweredDashAudioID);
			g_PlayerEmpoweredDashAudioID = -1;
		}
		if (g_PlayerDamageAudioID >= 0)
		{
			UnloadAudio(g_PlayerDamageAudioID);
			g_PlayerDamageAudioID = -1;
		}
		if (g_PlayerLevelUpAudioID >= 0)
		{
			UnloadAudio(g_PlayerLevelUpAudioID);
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
		g_PlayerEmpoweredDashCharges = PlayerDash::EmpoweredCharges;
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
		// Spawn the first footprint immediately once fast running begins.
		g_PlayerSprintDustElapsed = is_sprinting ?
			PlayerMovement::SprintDustInterval : 0.0f;
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

	bool IsEmpoweredDashModeActive()
	{
		return g_PlayerEmpoweredDashModeActive &&
			g_PlayerEmpoweredDashCharges > 0;
	}

	int GetEmpoweredDashCharges()
	{
		return IsEmpoweredDashModeActive() ? g_PlayerEmpoweredDashCharges : 0;
	}

	void Update(float delta_time)
	{
		UpdateSlashTrails(delta_time);
		g_PlayerDamageInvincibilityTimer = std::max(
			0.0f,
			g_PlayerDamageInvincibilityTimer - std::max(delta_time, 0.0f));
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
			g_PlayerSprintDustElapsed += std::max(delta_time, 0.0f);
			while (g_PlayerSprintDustElapsed >= PlayerMovement::SprintDustInterval)
			{
				g_PlayerSprintDustElapsed -= PlayerMovement::SprintDustInterval;
				cGameEffectManager::GetInstance().Play(
					GameEffectType::SprintDust,
					{ g_PlayerPos.x, g_PlayerPos.y + PlayerMovement::FeetYOffset },
					0.92f,
					{ 0.90f, 0.86f, 0.76f, 1.0f });
			}
		}
		else if (!has_move_input)
		{
			g_PlayerSprintDustElapsed = PlayerMovement::SprintDustInterval;
		}
		if (has_move_input && move.x != 0.0f)
		{
			g_PlayerFacingLeft = move.x < 0.0f;
		}
		const int animation_frame_count = g_PlayerIsMoving ?
			PlayerAnimation::Run.FrameCount : PlayerAnimation::Idle.FrameCount;
		const float animation_frame_duration = g_PlayerIsMoving ?
			PlayerAnimation::Run.FrameDuration : PlayerAnimation::Idle.FrameDuration;
		g_PlayerAnimTimer += std::max(delta_time, 0.0f);
		while (g_PlayerAnimTimer >= animation_frame_duration)
		{
			g_PlayerAnimTimer -= animation_frame_duration;
			g_PlayerCurrentFrame =
				(g_PlayerCurrentFrame + 1) % animation_frame_count;
		}

		const bool dash_requested = g_PlayerDashRequested;
		g_PlayerDashRequested = false;
		const bool can_aim_dash = g_PlayerEmpoweredDashModeActive &&
			g_PlayerEmpoweredDashCharges > 0;
		if (dash_requested && g_PlayerDashCooldown <= 0.0f &&
			!g_PlayerDashActive && (has_move_input || can_aim_dash))
		{
			const XMFLOAT2 dash_direction = has_move_input ? move : g_PlayerAimDirection;
			g_PlayerDashStartPos = g_PlayerPos;
			g_PlayerDashEndPos = g_PlayerPos;
			g_PlayerDashDirection = dash_direction;
			g_PlayerDashElapsed = 0.0f;
			g_PlayerDashActive = true;
			const int dash_audio_id = can_aim_dash ?
				g_PlayerEmpoweredDashAudioID : g_PlayerDashAudioID;
			if (dash_audio_id >= 0)
			{
				PlayAudio(dash_audio_id);
			}
			g_PlayerDashIsEmpowered = can_aim_dash;
			g_PlayerDashEffectIsEmpowered = g_PlayerDashIsEmpowered;
			g_PlayerDashCooldown = g_PlayerDashIsEmpowered ?
				PlayerDash::EmpoweredCooldown : PlayerDash::Cooldown;
			g_PlayerDashEffectTime = g_PlayerDashIsEmpowered ?
				PlayerDash::EmpoweredEffectDuration : PlayerDash::EffectDuration;
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
			const float dash_duration = g_PlayerDashIsEmpowered ?
				PlayerDash::EmpoweredDuration : PlayerDash::Duration;
			const float dash_distance = g_PlayerDashIsEmpowered ?
				PlayerDash::EmpoweredDistance : PlayerDash::Distance;
			const float previous_progress = g_PlayerDashElapsed / dash_duration;
			g_PlayerDashElapsed = std::min(
				g_PlayerDashElapsed + std::max(delta_time, 0.0f),
				dash_duration);
			const float current_progress = g_PlayerDashElapsed / dash_duration;
			const float distance = dash_distance *
				(GetDashDistanceRatio(current_progress) -
					GetDashDistanceRatio(previous_progress));
			const XMFLOAT2 dash_movement = {
				g_PlayerDashDirection.x * distance,
				g_PlayerDashDirection.y * distance,
			};
			g_PlayerPos = ProceduralMap_MoveActorCircle(
				g_PlayerPos, dash_movement, PlayerMovement::CollisionRadius);
			g_PlayerDashEndPos = g_PlayerPos;
			if (g_PlayerDashElapsed >= dash_duration)
			{
				g_PlayerDashActive = false;
				g_PlayerDashElapsed = PlayerDash::Duration;
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
			const float move_speed = g_PlayerSpeed *
				(g_PlayerIsSprinting ? PlayerMovement::SprintSpeedMultiplier : 1.0f);
			const XMFLOAT2 movement = {
				move.x * move_speed * delta_time,
				move.y * move_speed * delta_time,
			};
			g_PlayerPos = ProceduralMap_MoveActorCircle(
				g_PlayerPos, movement, PlayerMovement::CollisionRadius);
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
		const float aim_length_sq = aim.x * aim.x + aim.y * aim.y;
		if (aim_length_sq <= PlayerMovement::AimDeadZoneSq)
		{
			return;
		}
		const float inv_aim_length = 1.0f / std::sqrt(aim_length_sq);
		g_PlayerAimDirection = {
			aim.x * inv_aim_length,
			aim.y * inv_aim_length,
		};
	}

	void ApplyDamage(float damage)
	{
		if (damage <= 0.0f || g_PlayerHitPoint <= 0.0f ||
			g_PlayerDamageInvincibilityTimer > 0.0f ||
			g_PlayerDashActive)
		{
			return;
		}

		g_PlayerHitPoint = std::clamp(
			g_PlayerHitPoint - damage,
			0.0f,
			g_PlayerMaxHitPoint);
		g_PlayerDamageInvincibilityTimer = PlayerStats::DamageInvincibleDuration;
		if (g_PlayerDamageAudioID >= 0)
		{
			PlayAudio(g_PlayerDamageAudioID);
		}
		GameDamageText::Spawn(
			damage,
			g_PlayerPos,
			{ 1.0f, 0.12f, 0.08f, 1.0f });
		Blood::Spawn(g_PlayerPos);
	}

	float Heal(float amount)
	{
		if (amount <= 0.0f || g_PlayerHitPoint <= 0.0f)
		{
			return 0.0f;
		}

		const float previous_hit_point = g_PlayerHitPoint;
		g_PlayerHitPoint = std::min(
			g_PlayerHitPoint + amount,
			g_PlayerMaxHitPoint);
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
			PlayAudio(g_PlayerLevelUpAudioID);
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
		g_PlayerDieCurrentFrame = std::min(
			static_cast<int>(g_PlayerDieAnimTimer / PlayerAnimation::Die.FrameDuration),
			PlayerAnimation::Die.FrameCount - 1);
		if (g_PlayerDieAnimTimer >=
			PlayerAnimation::Die.FrameCount * PlayerAnimation::Die.FrameDuration)
		{
			g_PlayerDieCurrentFrame = PlayerAnimation::Die.FrameCount - 1;
			g_PlayerDieAnimationFinished = true;
		}
	}

	bool IsDeathAnimationFinished()
	{
		return g_PlayerDieAnimationFinished;
	}

	void RegisterCollider()
	{
		CollisionSystem_RegisterCircle(
			0,
			CollisionLayer::Player,
			CollisionLayer::Enemy | CollisionLayer::EnemyBullet,
			g_PlayerPos,
			PlayerMovement::CollisionRadius,
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
				(hit->BodyA.Layer == CollisionLayer::Enemy &&
					hit->BodyB.Layer == CollisionLayer::Player) ||
				(hit->BodyA.Layer == CollisionLayer::Player &&
					hit->BodyB.Layer == CollisionLayer::Enemy);
			if (enemy_hit_player)
			{
				ApplyDamage(PlayerStats::ContactDamage);
				return;
			}

			int enemy_bullet_id = COLLISION_INVALID_ID;
			if (hit->BodyA.Layer == CollisionLayer::EnemyBullet &&
				hit->BodyB.Layer == CollisionLayer::Player)
			{
				enemy_bullet_id = hit->BodyA.OwnerID;
			}
			else if (hit->BodyA.Layer == CollisionLayer::Player &&
				hit->BodyB.Layer == CollisionLayer::EnemyBullet)
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
			const bool lighting_was_enabled =
				Sprite_SetLightingEnabled(false);
			DrawSlashTrails();
			Sprite_SetLightingEnabled(lighting_was_enabled);
		}
		const int texture_id = g_PlayerDieAnimationVisible ?
			g_PlayerDieTextureID :
			(g_PlayerIsMoving ? g_PlayerRunTextureID : g_PlayerIdleTextureID);
		const int frame_width = g_PlayerDieAnimationVisible ?
			PlayerAnimation::Die.FrameWidth :
			(g_PlayerIsMoving ? PlayerAnimation::Run.FrameWidth : PlayerAnimation::Idle.FrameWidth);
		const int frame_height = g_PlayerDieAnimationVisible ?
			PlayerAnimation::Die.FrameHeight :
			(g_PlayerIsMoving ? PlayerAnimation::Run.FrameHeight : PlayerAnimation::Idle.FrameHeight);
		const int current_frame = g_PlayerDieAnimationVisible ?
			g_PlayerDieCurrentFrame : g_PlayerCurrentFrame;
		const int frame_x = current_frame * frame_width;
		const float unsigned_draw_width = frame_width * PlayerAnimation::SpriteScale;
		const float draw_width = g_PlayerFacingLeft ?
			-unsigned_draw_width : unsigned_draw_width;
		const float draw_height = frame_height * PlayerAnimation::SpriteScale;
		const float draw_y = g_PlayerPos.y +
			(g_PlayerIsMoving && !g_PlayerDieAnimationVisible ?
				PlayerAnimation::RunDrawYOffset : 0.0f);
		if (g_PlayerDieAnimationVisible)
		{
			Sprite_DrawRegion(
				texture_id,
				g_PlayerPos.x,
				draw_y,
				draw_width,
				draw_height,
				frame_x, 0, frame_width, frame_height,
				{ 1.0f, 1.0f, 1.0f, 1.0f });
			return;
		}
		const bool damage_flash = g_PlayerDamageInvincibilityTimer >
			PlayerStats::DamageInvincibleDuration - PlayerStats::DamageFlashDuration;
		const XMFLOAT4 player_color = damage_flash ?
			XMFLOAT4{ 1.0f, 0.15f, 0.15f, 1.0f } :
			XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };

		if (g_PlayerDashEffectTime > 0.0f)
		{
			const float effect_duration = g_PlayerDashEffectIsEmpowered ?
				PlayerDash::EmpoweredEffectDuration : PlayerDash::EffectDuration;
			const float fade = std::clamp(
				g_PlayerDashEffectTime / effect_duration,
				0.0f,
				1.0f);
			const bool lighting_was_enabled =
				Sprite_SetLightingEnabled(false);
			for (int i = 0; i < PlayerDash::AfterimageCount; ++i)
			{
				const float color_amount = static_cast<float>(i) /
					static_cast<float>(PlayerDash::AfterimageCount - 1);
				const float path_amount = static_cast<float>(i) /
					static_cast<float>(PlayerDash::AfterimageCount);
				const XMFLOAT2 afterimage_position = {
					g_PlayerDashStartPos.x +
						(g_PlayerDashEndPos.x - g_PlayerDashStartPos.x) * path_amount,
					g_PlayerDashStartPos.y +
						(g_PlayerDashEndPos.y - g_PlayerDashStartPos.y) * path_amount,
				};
				// Keep the oldest silhouettes purple and shift toward electric blue
				// near the player. The stronger alpha keeps the dash readable on
				// both the dark ground and bright combat effects.
				const XMFLOAT4 afterimage_color = {
					0.72f - 0.46f * color_amount,
					0.30f + 0.42f * color_amount,
					1.0f,
					fade * (0.24f + 0.30f * color_amount),
				};
				Sprite_DrawRegion(
					texture_id,
					afterimage_position.x,
					afterimage_position.y + PlayerAnimation::RunDrawYOffset,
					draw_width,
					draw_height,
					frame_x, 0, frame_width, frame_height,
					afterimage_color);
			}
			Sprite_SetLightingEnabled(lighting_was_enabled);
			Sprite_DrawRegion(
				texture_id,
				g_PlayerPos.x,
				draw_y,
				draw_width,
				draw_height,
				frame_x, 0, frame_width, frame_height,
				damage_flash ? player_color : XMFLOAT4{ 0.58f, 0.76f, 1.0f, 1.0f });
			return;
		}

		Sprite_DrawRegion(
			texture_id,
			g_PlayerPos.x,
			draw_y,
			draw_width,
			draw_height,
			frame_x, 0, frame_width, frame_height,
			player_color);
	}

	void DrawMapMarker(
		const XMFLOAT2& map_origin,
		float world_scale,
		bool expanded)
	{
		const XMFLOAT2 marker_position = {
			map_origin.x + g_PlayerPos.x * world_scale,
			map_origin.y + g_PlayerPos.y * world_scale,
		};
		const float marker_size = expanded ? 12.0f : 8.0f;
		Sprite_DrawSized(
			g_PlayerSlashLaserTextureID,
			marker_position.x,
			marker_position.y,
			marker_size,
			marker_size,
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
}
