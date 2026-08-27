#ifndef GAME_HEALING_ITEM_H
#define GAME_HEALING_ITEM_H

#include <DirectXMath.h>

namespace GameHealingItem
{
	void Initialize();
	void Finalize();
	void Clear();
	void TrySpawn(
		const DirectX::XMFLOAT2& world_position,
		int room_index);
	void Update(float delta_time, const DirectX::XMFLOAT2& player_position);
	void Draw();
}

#endif // !GAME_HEALING_ITEM_H
