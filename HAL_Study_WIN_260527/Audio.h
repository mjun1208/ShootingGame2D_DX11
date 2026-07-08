#pragma once



void InitAudio();
void ReleaseAudio();


int LoadAudio(const char* FileName);
void UnloadAudio(int Index);
void PlayAudio(int Index, bool Loop = false);

