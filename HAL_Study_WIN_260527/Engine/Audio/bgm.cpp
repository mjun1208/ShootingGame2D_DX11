#include "bgm.h"

#include "Audio.h"
#include "math_utils.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace
{
	constexpr std::size_t TrackCount = static_cast<std::size_t>(BgmTrack::Count);
	// 음원 간 균형을 맞추는 고정값이며 사용자 음량 설정과는 별개다.
	constexpr float BaseGain = 0.65f;
	constexpr std::array<float, TrackCount> TrackGains{ 0.80f, 0.65f, 0.95f, 1.10f };
	constexpr std::array<const char*, TrackCount> TrackPaths{
		"asset/sound/bgm/title_ominous.wav",
		"asset/sound/bgm/stage1_forest.wav",
		"asset/sound/bgm/stage2_dungeon1.wav",
		"asset/sound/bgm/stage2_dungeon2.wav",
	};
	std::array<int, TrackCount> g_TrackAudioIDs = []
	{
		std::array<int, TrackCount> ids{};
		ids.fill(AudioInvalidID);
		return ids;
	}();
	BgmTrack g_CurrentTrack = BgmTrack::None;
	BgmTrack g_PreviousTrack = BgmTrack::None;
	float g_CurrentGain = 0.0f;
	float g_PreviousStartGain = 0.0f;
	float g_FadeElapsed = 0.0f;
	float g_FadeDuration = 0.0f;

	int GetTrackAudioID(BgmTrack track)
	{
		const int index = static_cast<int>(track);
		return IsValidIndex(index, static_cast<int>(TrackCount)) ? g_TrackAudioIDs[index] : AudioInvalidID;
	}

	void StopTrack(BgmTrack& track)
	{
		Audio_Stop(GetTrackAudioID(track));
		track = BgmTrack::None;
	}

	void SetTrackGain(BgmTrack track, float gain)
	{
		const int audio_id = GetTrackAudioID(track);
		if (audio_id != AudioInvalidID)
		{
			Audio_SetGain(audio_id, gain * TrackGains[static_cast<std::size_t>(track)]);
		}
	}
}

void Bgm_Initialize()
{
	Bgm_Finalize();
	for (std::size_t index = 0; index < TrackCount; ++index)
	{
		g_TrackAudioIDs[index] = Audio_Load(TrackPaths[index]);
	}
}

void Bgm_Finalize()
{
	Bgm_Stop();
	for (int& audio_id : g_TrackAudioIDs)
	{
		Audio_Unload(audio_id);
		audio_id = AudioInvalidID;
	}
}

void Bgm_Update(float delta_time)
{
	delta_time = std::max(delta_time, 0.0f);
	if (g_CurrentTrack == BgmTrack::None && g_PreviousTrack == BgmTrack::None)
	{
		return;
	}
	// 페이드가 끝나면 음량을 더 갱신하지 않는다.
	if (g_FadeElapsed >= g_FadeDuration && g_PreviousTrack == BgmTrack::None)
	{
		return;
	}
	g_FadeElapsed += delta_time;
	const float fade_ratio = g_FadeDuration > 0.0f ? g_FadeElapsed / g_FadeDuration : 1.0f;
	const float blend = SmoothStep(fade_ratio);
	g_CurrentGain = g_CurrentTrack != BgmTrack::None ? BaseGain * blend : 0.0f;
	SetTrackGain(g_CurrentTrack, g_CurrentGain);
	SetTrackGain(g_PreviousTrack, g_PreviousStartGain * (1.0f - blend));
	if (fade_ratio >= 1.0f)
	{
		StopTrack(g_PreviousTrack);
		g_PreviousStartGain = 0.0f;
	}
}

void Bgm_Play(BgmTrack track, float fade_seconds)
{
	if (track == g_CurrentTrack)
	{
		return;
	}
	const float fade_duration = std::max(fade_seconds, 0.0f);
	if (track != BgmTrack::None)
	{
		const int audio_id = GetTrackAudioID(track);
		if (audio_id == AudioInvalidID)
		{
			return;
		}
		// 페이드 아웃 중인 곡으로 돌아가면 다시 재생하고 아래에서 중복으로 정지하지 않는다.
		if (track == g_PreviousTrack)
		{
			StopTrack(g_PreviousTrack);
		}
		SetTrackGain(track, fade_duration > 0.0f ? 0.0f : BaseGain);
		if (!Audio_Play(audio_id, true))
		{
			return;
		}
	}

	StopTrack(g_PreviousTrack);
	g_PreviousTrack = g_CurrentTrack;
	g_PreviousStartGain = g_CurrentGain;
	g_CurrentTrack = track;
	g_CurrentGain = track != BgmTrack::None && fade_duration <= 0.0f ? BaseGain : 0.0f;
	g_FadeElapsed = 0.0f;
	g_FadeDuration = fade_duration;
	if (g_FadeDuration <= 0.0f)
	{
		StopTrack(g_PreviousTrack);
		g_PreviousStartGain = 0.0f;
	}
}

void Bgm_Stop()
{
	StopTrack(g_PreviousTrack);
	StopTrack(g_CurrentTrack);
	g_CurrentGain = 0.0f;
	g_PreviousStartGain = 0.0f;
	g_FadeElapsed = 0.0f;
	g_FadeDuration = 0.0f;
}
