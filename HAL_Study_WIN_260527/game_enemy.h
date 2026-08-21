#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

#include <DirectXMath.h>
#include <cstdint>

class cChainLightning;

enum class MonsterType : std::uint8_t
{
	Slime = 1,
	SkeletonBase,
	SkeletonMage,
	SkeletonRogue,
	SkeletonWarrior,
	Bat,
};

namespace GameEnemy
{
	void Initialize();
	void Finalize();
	void ResetDungeon();
	void Update(float delta_time);
	bool FindNearestAlive(
		const DirectX::XMFLOAT2& origin,
		DirectX::XMFLOAT2& out_position);
	bool FindNearestAliveForChain(
		const DirectX::XMFLOAT2& origin,
		const int* excluded_enemy_ids,
		int excluded_count,
		float max_distance,
		int& out_enemy_id,
		DirectX::XMFLOAT2& out_position);
	bool ApplyChainLightningDamage(int enemy_id, float damage);
	bool IsRoundCleared();
	void DrawSpawnTelegraphs();
	void Draw();
	void DrawMapMarkers(
		const DirectX::XMFLOAT2& map_origin,
		float world_scale,
		bool expanded);
	void RegisterColliders();
	void HandleCollisionHits(cChainLightning& chain_lightning);
	void Deactivate(int enemy_id);
	bool IsActive(int enemy_id);
}

#endif // !GAME_ENEMY_H
