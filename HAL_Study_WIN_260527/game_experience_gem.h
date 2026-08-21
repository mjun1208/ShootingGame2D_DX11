#ifndef GAME_EXPERIENCE_GEM_H
#define GAME_EXPERIENCE_GEM_H

#include <DirectXMath.h>

namespace GameExperienceGem
{
	void Initialize();
	void Finalize();
	void Clear();
	void Spawn(const DirectX::XMFLOAT2& world_position, int experience);
	void Update(float delta_time, const DirectX::XMFLOAT2& player_position);
	void Draw();
}

#endif // !GAME_EXPERIENCE_GEM_H
