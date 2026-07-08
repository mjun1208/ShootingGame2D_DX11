#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <DirectXMath.h>
 
void Game_Player_Initialize();
void Game_Player_Finalize();
void Game_Player_Update(float delta_time);
void Game_Player_Draw();
DirectX::XMFLOAT2 Game_Player_GetBulletSpawnPosition();
DirectX::XMFLOAT2 Game_Player_GetAimDirection();
DirectX::XMFLOAT2 Game_Player_GetPosition();

#endif // !GAME_PLAYER_H
