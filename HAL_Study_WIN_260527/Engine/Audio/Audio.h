#ifndef AUDIO_H
#define AUDIO_H

inline constexpr int AudioInvalidID = -1;

// Bgm_Initialize 전에 초기화하고 Bgm_Finalize 뒤에 해제한다.
void Audio_Initialize();
void Audio_Finalize();

int Audio_Load(const char* file_name);
void Audio_Unload(int audio_id);
bool Audio_Play(int audio_id, bool loop = false);
void Audio_Stop(int audio_id);
// BGM 교차 페이드 등의 재생 효과에 사용하는 음원별 음량 배율.
void Audio_SetGain(int audio_id, float gain);

#endif // AUDIO_H
