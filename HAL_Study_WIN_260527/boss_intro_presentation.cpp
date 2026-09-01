#include "boss_intro_presentation.h"

#include "Audio.h"
#include "config.h"
#include "debug_text.h"
#include "direct3d.h"
#include "game_enemy.h"
#include "sprite.h"
#include "sprite_instanced.h"
#include "texture.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace
{
	namespace Intro
	{
		constexpr const char* WarningSoundPath =
			"asset/sound/pixabay-shockwave-105526.wav";
		constexpr int SourceY = 229;
		constexpr int SourceWidth = 2161;
		constexpr int SourceHeight = 251;
		constexpr float TextureHeight = 724.0f;
		constexpr float BandHeight = 148.0f;
		constexpr float UpperTargetY = SCREEN_HEIGHT * 0.29f;
		constexpr float LowerTargetY = SCREEN_HEIGHT * 0.71f;
		constexpr float SlideInDuration = 0.25f;
		constexpr float SlideOutStart = 1.55f;
		constexpr float SlideOutDuration = 0.28f;
		constexpr float TotalDuration = SlideOutStart + SlideOutDuration;
		constexpr float ScrollSpeed = 245.0f;
		constexpr float BlinkCyclesPerSecond = 3.0f;
		constexpr float Pi = 3.14159265358979323846f;
	}

	float SmoothStep01(float amount)
	{
		const float value = std::clamp(amount, 0.0f, 1.0f);
		return value * value * (3.0f - 2.0f * value);
	}

	std::unique_ptr<hal::DebugText> CreateBossNameText()
	{
		const char* name = GameEnemy::GetBossDisplayName();
		const std::size_t character_count = name ?
			std::char_traits<char>::length(name) : 0;
		constexpr float glyph_width = 40.0f;
		constexpr float glyph_height = 64.0f;
		constexpr float character_spacing = 52.0f;
		const float text_width = character_count > 0 ?
			glyph_width +
				(static_cast<float>(character_count) - 1.0f) * character_spacing :
			0.0f;
		return std::make_unique<hal::DebugText>(
			Direct3D_GetDevice(),
			Direct3D_GetDeviceContext(),
			L"asset/font/fixedsys/FixedsysExcelsior_ascii_320x512.png",
			SCREEN_WIDTH,
			SCREEN_HEIGHT,
			SCREEN_WIDTH * 0.5f - text_width * 0.5f,
			SCREEN_HEIGHT * 0.5f - glyph_height * 0.5f,
			1,
			0,
			glyph_height,
			character_spacing);
	}

	void DrawDangerBand(
		int texture_id,
		float center_y,
		float elapsed_time,
		float direction,
		float alpha)
	{
		if (texture_id == TEXTURE_INVALID_ID || alpha <= 0.0f)
		{
			return;
		}
		const float tile_width = Intro::SourceWidth *
			Intro::BandHeight / Intro::SourceHeight;
		const float uv_phase = std::fmod(
			std::max(elapsed_time, 0.0f) * Intro::ScrollSpeed / tile_width,
			1.0f);
		SpriteInstance instance{};
		instance.Position = { SCREEN_WIDTH * 0.5f, center_y };
		instance.Size = { static_cast<float>(SCREEN_WIDTH), Intro::BandHeight };
		instance.Color = { 1.0f, 1.0f, 1.0f, alpha };
		instance.TexcoordOffset = {
			-direction * uv_phase,
			Intro::SourceY / Intro::TextureHeight,
		};
		instance.TexcoordScale = {
			SCREEN_WIDTH / tile_width,
			Intro::SourceHeight / Intro::TextureHeight,
		};
		SpriteInstanced_DrawWrapUUnlit(texture_id, &instance, 1);
	}
}

struct BossIntroPresentation::Impl
{
	int DangerTextureID{ TEXTURE_INVALID_ID };
	int WarningAudioID{ -1 };
	std::unique_ptr<hal::DebugText> BossNameText;
	float ElapsedTime{ 0.0f };
	bool Active{ false };
	bool Played{ false };
};

BossIntroPresentation::BossIntroPresentation()
	: m_Impl(std::make_unique<Impl>())
{
}

BossIntroPresentation::~BossIntroPresentation() = default;

bool BossIntroPresentation::Initialize()
{
	Finalize();
	m_Impl->DangerTextureID = Texture_Load(
		L"asset/texture/ui/boss/boss_danger_tape_generated.png", false);
	if (m_Impl->DangerTextureID == TEXTURE_INVALID_ID)
	{
		return false;
	}
	m_Impl->WarningAudioID = LoadAudio(Intro::WarningSoundPath);
	Reset();
	return true;
}

void BossIntroPresentation::Finalize()
{
	m_Impl->BossNameText.reset();
	UnloadAudio(m_Impl->WarningAudioID);
	m_Impl->WarningAudioID = -1;
	Texture_Release(m_Impl->DangerTextureID);
	m_Impl->DangerTextureID = TEXTURE_INVALID_ID;
	m_Impl->ElapsedTime = 0.0f;
	m_Impl->Active = false;
	m_Impl->Played = false;
}

void BossIntroPresentation::Reset()
{
	m_Impl->BossNameText = CreateBossNameText();
	m_Impl->ElapsedTime = 0.0f;
	m_Impl->Active = false;
	m_Impl->Played = false;
}

bool BossIntroPresentation::Begin()
{
	if (m_Impl->Played || m_Impl->Active)
	{
		return false;
	}
	m_Impl->Played = true;
	m_Impl->Active = true;
	m_Impl->ElapsedTime = 0.0f;
	PlayAudio(m_Impl->WarningAudioID);
	return true;
}

void BossIntroPresentation::Update(float delta_time)
{
	if (!m_Impl->Active)
	{
		return;
	}
	m_Impl->ElapsedTime += std::max(delta_time, 0.0f);
	if (m_Impl->ElapsedTime >= Intro::TotalDuration)
	{
		m_Impl->ElapsedTime = Intro::TotalDuration;
		m_Impl->Active = false;
	}
}

void BossIntroPresentation::Draw(int overlay_texture_id)
{
	if (!m_Impl->Active || m_Impl->DangerTextureID == TEXTURE_INVALID_ID)
	{
		return;
	}
	const float elapsed = m_Impl->ElapsedTime;
	float visibility = 1.0f;
	if (elapsed < Intro::SlideInDuration)
	{
		visibility = SmoothStep01(elapsed / Intro::SlideInDuration);
	}
	else if (elapsed > Intro::SlideOutStart)
	{
		visibility = 1.0f - SmoothStep01(
			(elapsed - Intro::SlideOutStart) / Intro::SlideOutDuration);
	}
	const float blink = 0.5f + 0.5f * std::sin(
		elapsed * Intro::BlinkCyclesPerSecond * 2.0f * Intro::Pi);
	const float band_alpha = visibility * (0.72f + blink * 0.28f);
	const float offscreen_upper_y = -Intro::BandHeight * 0.5f - 16.0f;
	const float offscreen_lower_y =
		SCREEN_HEIGHT + Intro::BandHeight * 0.5f + 16.0f;
	const float upper_y = offscreen_upper_y +
		(Intro::UpperTargetY - offscreen_upper_y) * visibility;
	const float lower_y = offscreen_lower_y +
		(Intro::LowerTargetY - offscreen_lower_y) * visibility;
	Sprite_DrawSized(
		overlay_texture_id,
		SCREEN_WIDTH * 0.5f,
		SCREEN_HEIGHT * 0.5f,
		static_cast<float>(SCREEN_WIDTH),
		static_cast<float>(SCREEN_HEIGHT),
		{ 0.34f, 0.01f, 0.025f, visibility * (0.12f + blink * 0.10f) });
	DrawDangerBand(
		m_Impl->DangerTextureID, upper_y, elapsed, 1.0f, band_alpha);
	DrawDangerBand(
		m_Impl->DangerTextureID, lower_y, elapsed, -1.0f, band_alpha);
	if (m_Impl->BossNameText)
	{
		const float name_fade = SmoothStep01(
			(elapsed - Intro::SlideInDuration * 0.65f) / 0.22f);
		m_Impl->BossNameText->Clear();
		m_Impl->BossNameText->SetText(
			GameEnemy::GetBossDisplayName(),
			{ 1.0f, 0.88f, 0.72f,
				visibility * name_fade * (0.84f + blink * 0.16f) });
		m_Impl->BossNameText->Draw();
	}
}

bool BossIntroPresentation::IsActive() const
{
	return m_Impl->Active;
}

bool BossIntroPresentation::HasPlayed() const
{
	return m_Impl->Played;
}
