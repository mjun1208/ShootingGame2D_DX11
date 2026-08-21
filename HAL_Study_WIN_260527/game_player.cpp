#include "game_player.h"

#include "blood.h"
#include "collision.h"
#include "direct3d.h"
#include "shader.h"
#include "sprite.h"
#include "texture.h"
#include "input_keyboard.h"
#include "procedural_map.h"
#include <algorithm>
#include <cmath>

#include <DirectXMath.h>
#include "audio.h"
using namespace DirectX;

static int g_PlayerIdleTextureID = TEXTURE_INVALID_ID;
static int g_PlayerRunTextureID = TEXTURE_INVALID_ID;
static int g_PlayerDieTextureID = TEXTURE_INVALID_ID;

static XMFLOAT2 g_PlayerPos;
static XMFLOAT2 g_PlayerAimDirection = { 0.0f, -1.0f };

static constexpr float PLAYER_MAX_HIT_POINT = 100.0f;
static constexpr float PLAYER_CONTACT_DAMAGE = 10.0f;
static constexpr float PLAYER_DAMAGE_INVINCIBLE_DURATION = 0.65f;
static constexpr float PLAYER_DAMAGE_FLASH_DURATION = 0.12f;
static float g_PlayerHitPoint = PLAYER_MAX_HIT_POINT;
static float g_PlayerDamageInvincibilityTimer = 0.0f;
static int g_PlayerLevel = 1;
static int g_PlayerExperience = 0;

static float g_PlayerSpeed = 500.0f;

static constexpr float PLAYER_SPRITE_SCALE = 3.0f;
static constexpr int PLAYER_IDLE_FRAME_COUNT = 4;
static constexpr int PLAYER_IDLE_FRAME_WIDTH = 32;
static constexpr int PLAYER_IDLE_FRAME_HEIGHT = 32;
static constexpr float PLAYER_IDLE_FRAME_DURATION = 0.14f;
static constexpr int PLAYER_RUN_FRAME_COUNT = 6;
static constexpr int PLAYER_RUN_FRAME_WIDTH = 64;
static constexpr int PLAYER_RUN_FRAME_HEIGHT = 64;
static constexpr float PLAYER_RUN_FRAME_DURATION = 0.08f;
static constexpr float PLAYER_RUN_DRAW_Y_OFFSET = -16.0f * PLAYER_SPRITE_SCALE;
static constexpr int PLAYER_DIE_FRAME_COUNT = 6;
static constexpr int PLAYER_DIE_FRAME_WIDTH = 64;
static constexpr int PLAYER_DIE_FRAME_HEIGHT = 32;
static constexpr float PLAYER_DIE_FRAME_DURATION = 0.12f;

static int g_PlayerCurrentFrame = 0;
static float g_PlayerAnimTimer = 0.0f;
static bool g_PlayerIsMoving = false;
static bool g_PlayerFacingLeft = false;
static int g_PlayerDieCurrentFrame = 0;
static float g_PlayerDieAnimTimer = 0.0f;
static bool g_PlayerDieAnimationVisible = false;
static bool g_PlayerDieAnimationFinished = false;
static constexpr float PLAYER_AIM_DEAD_ZONE_SQ = 16.0f;
static constexpr float PLAYER_COLLISION_RADIUS = 30.0f;
static constexpr float PLAYER_DASH_DISTANCE = 240.0f;
static constexpr float PLAYER_DASH_DURATION = 0.18f;
static constexpr float PLAYER_DASH_COOLDOWN = 0.8f;
static constexpr float PLAYER_DASH_EFFECT_DURATION = 0.22f;
static constexpr int PLAYER_DASH_AFTERIMAGE_COUNT = 6;

static float g_PlayerDashCooldown = 0.0f;
static float g_PlayerDashElapsed = PLAYER_DASH_DURATION;
static float g_PlayerDashEffectTime = 0.0f;
static XMFLOAT2 g_PlayerDashStartPos = { 0.0f, 0.0f };
static XMFLOAT2 g_PlayerDashEndPos = { 0.0f, 0.0f };
static XMFLOAT2 g_PlayerDashDirection = { 0.0f, 0.0f };
static bool g_PlayerDashRequested = false;

static int g_SoundId = 0;

static int GetRequiredExperience(int level)
{
	return 5 + std::max(level - 1, 0) * 5;
}

static void ResetDashState()
{
	g_PlayerDashCooldown = 0.0f;
	g_PlayerDashElapsed = PLAYER_DASH_DURATION;
	g_PlayerDashEffectTime = 0.0f;
	g_PlayerDashStartPos = g_PlayerPos;
	g_PlayerDashEndPos = g_PlayerPos;
	g_PlayerDashDirection = { 0.0f, 0.0f };
	g_PlayerDashRequested = false;
}

static float GetDashDistanceRatio(float progress)
{
	progress = std::clamp(progress, 0.0f, 1.0f);
	const float remaining = 1.0f - progress;
	return 1.0f - remaining * remaining;
}

namespace GamePlayer
{
	void Initialize()
	{
		g_PlayerPos = { 0.0f, 0.0f };
		g_PlayerAimDirection = { 0.0f, -1.0f };
		g_PlayerHitPoint = PLAYER_MAX_HIT_POINT;
		g_PlayerDamageInvincibilityTimer = 0.0f;
		g_PlayerLevel = 1;
		g_PlayerExperience = 0;
		ResetDashState();
		g_PlayerIdleTextureID = Texture_Load(L"asset/npc/Rogue/Idle/Idle-Sheet.png");
		g_PlayerRunTextureID = Texture_Load(L"asset/npc/Rogue/Run/Run-Sheet.png");
		g_PlayerDieTextureID = Texture_Load(L"asset/npc/Rogue/Death/Death-Sheet.png");
		g_PlayerCurrentFrame = 0;
		g_PlayerAnimTimer = 0.0f;
		g_PlayerIsMoving = false;
		g_PlayerFacingLeft = false;
		g_PlayerDieCurrentFrame = 0;
		g_PlayerDieAnimTimer = 0.0f;
		g_PlayerDieAnimationVisible = false;
		g_PlayerDieAnimationFinished = false;

		g_SoundId = LoadAudio("asset/sound/test.wav");
	}

	void Finalize()
	{
		Texture_Release(g_PlayerIdleTextureID);
		Texture_Release(g_PlayerRunTextureID);
		Texture_Release(g_PlayerDieTextureID);
		g_PlayerIdleTextureID = TEXTURE_INVALID_ID;
		g_PlayerRunTextureID = TEXTURE_INVALID_ID;
		g_PlayerDieTextureID = TEXTURE_INVALID_ID;
	}

	void RequestDash()
	{
		g_PlayerDashRequested = true;
	}

	void Update(float delta_time)
	{
		g_PlayerDamageInvincibilityTimer = std::max(
			0.0f,
			g_PlayerDamageInvincibilityTimer - std::max(delta_time, 0.0f));
		g_PlayerDashCooldown = std::max(0.0f, g_PlayerDashCooldown - delta_time);
		g_PlayerDashEffectTime = std::max(0.0f, g_PlayerDashEffectTime - delta_time);

		XMFLOAT2 move = { 0.0f, 0.0f };

		if (InputKeyboard_IsPress(KK_W))
		{
			PlayAudio(g_SoundId);
			move.y -= 1.0f;
		}
		if (InputKeyboard_IsPress(KK_S))
		{
			move.y += 1.0f;
		}
		if (InputKeyboard_IsPress(KK_A))
		{
			move.x -= 1.0f;
		}
		if (InputKeyboard_IsPress(KK_D))
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
		if (has_move_input && move.x != 0.0f)
		{
			g_PlayerFacingLeft = move.x < 0.0f;
		}
		const int animation_frame_count = g_PlayerIsMoving ?
			PLAYER_RUN_FRAME_COUNT : PLAYER_IDLE_FRAME_COUNT;
		const float animation_frame_duration = g_PlayerIsMoving ?
			PLAYER_RUN_FRAME_DURATION : PLAYER_IDLE_FRAME_DURATION;
		g_PlayerAnimTimer += std::max(delta_time, 0.0f);
		while (g_PlayerAnimTimer >= animation_frame_duration)
		{
			g_PlayerAnimTimer -= animation_frame_duration;
			g_PlayerCurrentFrame =
				(g_PlayerCurrentFrame + 1) % animation_frame_count;
		}

		const bool dash_requested = g_PlayerDashRequested;
		g_PlayerDashRequested = false;
		if (dash_requested && g_PlayerDashCooldown <= 0.0f &&
			g_PlayerDashElapsed >= PLAYER_DASH_DURATION && has_move_input)
		{
			g_PlayerDashStartPos = g_PlayerPos;
			g_PlayerDashEndPos = g_PlayerPos;
			g_PlayerDashDirection = move;
			g_PlayerDashElapsed = 0.0f;
			g_PlayerDashCooldown = PLAYER_DASH_COOLDOWN;
			g_PlayerDashEffectTime = PLAYER_DASH_EFFECT_DURATION;
			g_PlayerAimDirection = move;
		}

		if (g_PlayerDashElapsed < PLAYER_DASH_DURATION)
		{
			const float previous_progress = g_PlayerDashElapsed / PLAYER_DASH_DURATION;
			g_PlayerDashElapsed = std::min(
				g_PlayerDashElapsed + std::max(delta_time, 0.0f),
				PLAYER_DASH_DURATION);
			const float current_progress = g_PlayerDashElapsed / PLAYER_DASH_DURATION;
			const float distance = PLAYER_DASH_DISTANCE *
				(GetDashDistanceRatio(current_progress) -
					GetDashDistanceRatio(previous_progress));
			const XMFLOAT2 dash_movement = {
				g_PlayerDashDirection.x * distance,
				g_PlayerDashDirection.y * distance,
			};
			g_PlayerPos = ProceduralMap_MoveActorCircle(
				g_PlayerPos, dash_movement, PLAYER_COLLISION_RADIUS);
			g_PlayerDashEndPos = g_PlayerPos;
			return;
		}

		if (has_move_input)
		{
			const XMFLOAT2 movement = {
				move.x * g_PlayerSpeed * delta_time,
				move.y * g_PlayerSpeed * delta_time,
			};
			g_PlayerPos = ProceduralMap_MoveActorCircle(
				g_PlayerPos, movement, PLAYER_COLLISION_RADIUS);
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
		if (aim_length_sq <= PLAYER_AIM_DEAD_ZONE_SQ)
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
			g_PlayerDashElapsed < PLAYER_DASH_DURATION)
		{
			return;
		}

		g_PlayerHitPoint = std::clamp(
			g_PlayerHitPoint - damage,
			0.0f,
			PLAYER_MAX_HIT_POINT);
		g_PlayerDamageInvincibilityTimer = PLAYER_DAMAGE_INVINCIBLE_DURATION;
		Blood::Spawn(g_PlayerPos);
	}

	void AddExperience(int experience)
	{
		if (experience <= 0)
		{
			return;
		}

		g_PlayerExperience += experience;
		int required_experience = GetRequiredExperience(g_PlayerLevel);
		while (g_PlayerExperience >= required_experience)
		{
			g_PlayerExperience -= required_experience;
			++g_PlayerLevel;
			required_experience = GetRequiredExperience(g_PlayerLevel);
		}
	}

	float GetHitPoint()
	{
		return g_PlayerHitPoint;
	}

	float GetMaxHitPoint()
	{
		return PLAYER_MAX_HIT_POINT;
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
			static_cast<int>(g_PlayerDieAnimTimer / PLAYER_DIE_FRAME_DURATION),
			PLAYER_DIE_FRAME_COUNT - 1);
		if (g_PlayerDieAnimTimer >=
			PLAYER_DIE_FRAME_COUNT * PLAYER_DIE_FRAME_DURATION)
		{
			g_PlayerDieCurrentFrame = PLAYER_DIE_FRAME_COUNT - 1;
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
			CollisionLayer::Enemy,
			g_PlayerPos,
			PLAYER_COLLISION_RADIUS,
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
				ApplyDamage(PLAYER_CONTACT_DAMAGE);
				return;
			}
		}
	}

	void Draw()
	{
		const int texture_id = g_PlayerDieAnimationVisible ?
			g_PlayerDieTextureID :
			(g_PlayerIsMoving ? g_PlayerRunTextureID : g_PlayerIdleTextureID);
		const int frame_width = g_PlayerDieAnimationVisible ?
			PLAYER_DIE_FRAME_WIDTH :
			(g_PlayerIsMoving ? PLAYER_RUN_FRAME_WIDTH : PLAYER_IDLE_FRAME_WIDTH);
		const int frame_height = g_PlayerDieAnimationVisible ?
			PLAYER_DIE_FRAME_HEIGHT :
			(g_PlayerIsMoving ? PLAYER_RUN_FRAME_HEIGHT : PLAYER_IDLE_FRAME_HEIGHT);
		const int current_frame = g_PlayerDieAnimationVisible ?
			g_PlayerDieCurrentFrame : g_PlayerCurrentFrame;
		const int frame_x = current_frame * frame_width;
		const float unsigned_draw_width = frame_width * PLAYER_SPRITE_SCALE;
		const float draw_width = g_PlayerFacingLeft ?
			-unsigned_draw_width : unsigned_draw_width;
		const float draw_height = frame_height * PLAYER_SPRITE_SCALE;
		const float draw_y = g_PlayerPos.y +
			(g_PlayerIsMoving && !g_PlayerDieAnimationVisible ?
				PLAYER_RUN_DRAW_Y_OFFSET : 0.0f);
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
			PLAYER_DAMAGE_INVINCIBLE_DURATION - PLAYER_DAMAGE_FLASH_DURATION;
		const XMFLOAT4 player_color = damage_flash ?
			XMFLOAT4{ 1.0f, 0.15f, 0.15f, 1.0f } :
			XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };

		if (g_PlayerDashEffectTime > 0.0f)
		{
			const float fade = std::clamp(
				g_PlayerDashEffectTime / PLAYER_DASH_EFFECT_DURATION,
				0.0f,
				1.0f);
			for (int i = 0; i < PLAYER_DASH_AFTERIMAGE_COUNT; ++i)
			{
				const float color_amount = static_cast<float>(i) /
					static_cast<float>(PLAYER_DASH_AFTERIMAGE_COUNT - 1);
				const float path_amount = static_cast<float>(i) /
					static_cast<float>(PLAYER_DASH_AFTERIMAGE_COUNT);
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
					afterimage_position.y + PLAYER_RUN_DRAW_Y_OFFSET,
					draw_width,
					draw_height,
					frame_x, 0, frame_width, frame_height,
					afterimage_color);
			}
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
		const float marker_width = expanded ? 30.0f : 18.0f;
		const float marker_height = expanded ? 18.0f : 11.0f;
		Sprite_DrawRegion(
			g_PlayerIdleTextureID,
			marker_position.x,
			marker_position.y,
			marker_width,
			marker_height,
			0, 0, PLAYER_IDLE_FRAME_WIDTH, PLAYER_IDLE_FRAME_HEIGHT,
			{ 0.55f, 0.95f, 1.0f, 1.0f });
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
