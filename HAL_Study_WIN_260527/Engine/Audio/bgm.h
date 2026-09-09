#ifndef BGM_H
#define BGM_H

enum class BgmTrack
{
	None = -1,
	Title,
	Forest,
	Dungeon1,
	Dungeon2,
	Count,
};

// Audio는 재생 자원을, BGM은 곡 선택과 교차 페이드를 관리한다.
// Audio_Initialize 이후, Audio_Finalize 이전에 호출한다.
void Bgm_Initialize();
void Bgm_Finalize();
void Bgm_Update(float delta_time);
void Bgm_Play(BgmTrack track, float fade_seconds = 0.8f);
void Bgm_Stop();

#endif // BGM_H
