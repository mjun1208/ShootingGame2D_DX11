#ifndef GAME_EFFECT_H
#define GAME_EFFECT_H

#include <DirectXMath.h>

void Game_Effect_Initialize();
void Game_Effect_Finalize();
void Game_Effect_Update(float delta_time);
void Game_Effect_Draw();
void Game_Effect_PlayExplosion(const DirectX::XMFLOAT2& position);

#endif // !GAME_EFFECT_H
