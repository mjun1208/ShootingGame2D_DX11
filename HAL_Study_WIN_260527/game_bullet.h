#ifndef GAME_BULLET_H
#define GAME_BULLET_H

#include <DirectXMath.h>

void Game_Bullet_Initialize();
void Game_Bullet_Finalize();
void Game_Bullet_Fire(const DirectX::XMFLOAT2& spawn_position, const DirectX::XMFLOAT2& direction);
void Game_Bullet_Update(float delta_time);
void Game_Bullet_Draw();
void Game_Bullet_RegisterColliders();

#endif // !GAME_BULLET_H
