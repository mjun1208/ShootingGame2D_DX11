#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <DirectXMath.h>
 
void Game_Player_Initialize();
void Game_Player_Finalize();
void Game_Player_Update(float delta_time);
void Game_Player_Draw();
DirectX::XMFLOAT2 Game_Player_GetBulletSpawnPosition();

#endif // !GAME_PLAYER_H
