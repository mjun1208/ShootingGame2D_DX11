#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

void Game_Enemy_Initialize();
void Game_Enemy_Finalize();
void Game_Enemy_Update(float delta_time);
void Game_Enemy_Draw();
void Game_Enemy_RegisterColliders();
void Game_Enemy_HandleCollisionHits();
void Game_Enemy_Deactivate(int enemy_id);
bool Game_Enemy_IsActive(int enemy_id);

#endif // !GAME_ENEMY_H
