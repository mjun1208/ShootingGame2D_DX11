#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

#include "sprite_lighting.h"

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
	Orc,
	OrcRogue,
	OrcShaman,
	OrcWarrior,
	BossSlime,
	BossCorruptedKnight,
	BossCorruptedMage,
	BossCthulhu,
};

namespace GameEnemy
{
	struct BossDefeatPresentation
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 DrawSize{};
	};

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
	bool TryGetAlivePosition(
		int enemy_id,
		DirectX::XMFLOAT2& out_position);
	bool ApplyChainLightningDamage(int enemy_id, float damage);
	int ApplyDashSlashDamage(
		const DirectX::XMFLOAT2& start,
		const DirectX::XMFLOAT2& end,
		float half_width,
		float damage);
	void UpdateDashSlashAttacks(float delta_time);
	bool IsRoomDiscovered(int room_index);
	bool IsRoomCleared(int room_index);
	bool IsRoundCleared();
	bool HasPendingBossSpawn();
	bool ConsumeBossDefeatPresentation(BossDefeatPresentation& out_presentation);
	void FinishBossDefeatPresentation();
	bool TryGetBossHealth(float& out_hit_point, float& out_max_hit_point);
	const char* GetBossDisplayName();
	void DrawSpawnTelegraphs();
	void Draw();
	void DrawProjectiles();
	int AppendPointLights(
		SpritePointLight* lights,
		int light_count,
		int capacity,
		const DirectX::XMFLOAT2& camera_position,
		const DirectX::XMFLOAT2& viewport_size);
	void RegisterColliders();
	void HandleCollisionHits(cChainLightning& chain_lightning);
	bool ConsumeEnemyBullet(int projectile_id, float& out_damage);
	void Deactivate(int enemy_id);
	bool IsActive(int enemy_id);
}

#endif // !GAME_ENEMY_H
