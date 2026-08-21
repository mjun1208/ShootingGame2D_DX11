#ifndef GAME_DAMAGE_TEXT_H
#define GAME_DAMAGE_TEXT_H

#include <DirectXMath.h>

namespace GameDamageText
{
	void Initialize();
	void Finalize();
	void Clear();
	void Spawn(float damage, const DirectX::XMFLOAT2& world_position);
	void Update(float delta_time);
	void Draw();
}

#endif // !GAME_DAMAGE_TEXT_H
