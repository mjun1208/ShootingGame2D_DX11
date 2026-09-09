#include "Audio.h"

#include "math_utils.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#pragma comment(lib, "winmm.lib")

namespace
{
	// 기존 총 용량인 효과음 100개와 BGM 4곡을 유지한다.
	constexpr int AudioCapacity = 104;
	struct AudioClip
	{
		IXAudio2SourceVoice* Voice{};
		std::vector<BYTE> Data;
		UINT32 FrameCount{};
	};
	IXAudio2* g_XAudio{};
	IXAudio2MasteringVoice* g_MasteringVoice{};
	std::array<AudioClip, AudioCapacity> g_Clips{};

	AudioClip* GetClip(int audio_id)
	{
		return audio_id >= 0 && audio_id < AudioCapacity ? &g_Clips[audio_id] : nullptr;
	}

	bool ReadWave(const char* file_name, WAVEFORMATEXTENSIBLE& format, std::vector<BYTE>& data)
	{
		MMIOINFO info{};
		HMMIO file = mmioOpenA(const_cast<char*>(file_name), &info, MMIO_READ);
		if (!file)
		{
			return false;
		}
		const bool loaded = [&]()
		{
			MMCKINFO riff_chunk{};
			riff_chunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
			if (mmioDescend(file, &riff_chunk, nullptr, MMIO_FINDRIFF) != MMSYSERR_NOERROR)
			{
				return false;
			}
			MMCKINFO format_chunk{};
			format_chunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
			if (mmioDescend(file, &format_chunk, &riff_chunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR ||
			    format_chunk.cksize < sizeof(PCMWAVEFORMAT) || format_chunk.cksize > sizeof(format) ||
			    (format_chunk.cksize > sizeof(PCMWAVEFORMAT) && format_chunk.cksize < sizeof(WAVEFORMATEX)))
			{
				return false;
			}
			const LONG format_size = static_cast<LONG>(format_chunk.cksize);
			if (mmioRead(file, reinterpret_cast<HPSTR>(&format), format_size) != format_size ||
			    format.Format.nBlockAlign == 0 ||
			    (format_chunk.cksize >= sizeof(WAVEFORMATEX) &&
			     sizeof(WAVEFORMATEX) + format.Format.cbSize > format_chunk.cksize))
			{
				return false;
			}
			if (mmioAscend(file, &format_chunk, 0) != MMSYSERR_NOERROR)
			{
				return false;
			}
			MMCKINFO data_chunk{};
			data_chunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
			if (mmioDescend(file, &data_chunk, &riff_chunk, MMIO_FINDCHUNK) != MMSYSERR_NOERROR ||
			    data_chunk.cksize == 0 || data_chunk.cksize > static_cast<DWORD>((std::numeric_limits<LONG>::max)()) ||
			    data_chunk.cksize % format.Format.nBlockAlign != 0)
			{
				return false;
			}
			data.resize(data_chunk.cksize);
			const LONG byte_count = static_cast<LONG>(data_chunk.cksize);
			return mmioRead(file, reinterpret_cast<HPSTR>(data.data()), byte_count) == byte_count;
		}();
		mmioClose(file, 0);
		return loaded;
	}
}

void Audio_Initialize()
{
	if (g_XAudio)
	{
		return;
	}
	if (FAILED(XAudio2Create(&g_XAudio, 0)))
	{
		g_XAudio = nullptr;
		return;
	}
	if (FAILED(g_XAudio->CreateMasteringVoice(&g_MasteringVoice)))
	{
		g_XAudio->Release();
		g_XAudio = nullptr;
		g_MasteringVoice = nullptr;
	}
}

void Audio_Finalize()
{
	for (int audio_id = 0; audio_id < AudioCapacity; ++audio_id)
	{
		Audio_Unload(audio_id);
	}
	if (g_MasteringVoice)
	{
		g_MasteringVoice->DestroyVoice();
		g_MasteringVoice = nullptr;
	}
	if (g_XAudio)
	{
		g_XAudio->Release();
		g_XAudio = nullptr;
	}
}

int Audio_Load(const char* file_name)
{
	if (!g_XAudio || !file_name)
	{
		return AudioInvalidID;
	}
	const auto free_clip = std::find_if(g_Clips.begin(), g_Clips.end(),
	                                    [](const AudioClip& clip)
	                                    {
		                                    return !clip.Voice;
	                                    });
	if (free_clip == g_Clips.end())
	{
		return AudioInvalidID;
	}
	WAVEFORMATEXTENSIBLE format{};
	if (!ReadWave(file_name, format, free_clip->Data) ||
	    FAILED(g_XAudio->CreateSourceVoice(&free_clip->Voice, &format.Format)))
	{
		Audio_Unload(static_cast<int>(free_clip - g_Clips.begin()));
		return AudioInvalidID;
	}
	free_clip->FrameCount = static_cast<UINT32>(free_clip->Data.size()) / format.Format.nBlockAlign;
	return static_cast<int>(free_clip - g_Clips.begin());
}

void Audio_Unload(int audio_id)
{
	AudioClip* clip = GetClip(audio_id);
	if (!clip)
	{
		return;
	}
	if (clip->Voice)
	{
		clip->Voice->Stop();
		clip->Voice->DestroyVoice();
	}
	*clip = {};
}

void Audio_Stop(int audio_id)
{
	AudioClip* clip = GetClip(audio_id);
	if (clip && clip->Voice)
	{
		clip->Voice->Stop();
		clip->Voice->FlushSourceBuffers();
	}
}

bool Audio_Play(int audio_id, bool loop)
{
	AudioClip* clip = GetClip(audio_id);
	if (!clip || !clip->Voice || clip->Data.empty() || clip->FrameCount == 0)
	{
		return false;
	}
	Audio_Stop(audio_id);
	XAUDIO2_BUFFER buffer{};
	buffer.AudioBytes = static_cast<UINT32>(clip->Data.size());
	buffer.pAudioData = clip->Data.data();
	buffer.PlayLength = clip->FrameCount;
	if (loop)
	{
		buffer.LoopLength = clip->FrameCount;
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}
	if (FAILED(clip->Voice->SubmitSourceBuffer(&buffer)))
	{
		return false;
	}
	if (FAILED(clip->Voice->Start()))
	{
		Audio_Stop(audio_id);
		return false;
	}
	return true;
}

void Audio_SetGain(int audio_id, float gain)
{
	AudioClip* clip = GetClip(audio_id);
	if (clip && clip->Voice)
	{
		clip->Voice->SetVolume(Saturate(gain));
	}
}
