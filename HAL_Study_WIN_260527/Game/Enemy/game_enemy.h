#ifndef GAME_ENEMY_H
#define GAME_ENEMY_H

#include "monster_type.h"
#include "sprite_lighting.h"

#include <DirectXMath.h>

class cChainLightning;

namespace GameEnemy
{
	inline constexpr int ENEMY_CAPACITY = 512;

	constexpr bool IsValidEnemyID(int enemy_id)
	{
		return enemy_id >= 0 && enemy_id < ENEMY_CAPACITY;
	}

	struct BossDefeatedEvent
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 DrawSize{};
	};

	struct CombatFeedback
	{
		DirectX::XMFLOAT2 Position{};
		DirectX::XMFLOAT2 Direction{};
		float StrongestDamage{ 0.0f };
		int HitCount{ 0 };
		int KillCount{ 0 };
		bool HeavyHit{ false };
		bool BossHit{ false };
		bool BossKilled{ false };
	};

	void Initialize();
	void Finalize();
	void ResetDungeon();
	void Update(float delta_time);
	bool FindNearestAlive(const DirectX::XMFLOAT2& origin, DirectX::XMFLOAT2& out_position);
	bool FindNearestAliveForChain(const DirectX::XMFLOAT2& origin, const int* excluded_enemy_ids, int excluded_count,
	                              float max_distance, int& out_enemy_id, DirectX::XMFLOAT2& out_position);
	bool IsAliveAndTargetable(int enemy_id);
	bool TryGetAlivePosition(int enemy_id, DirectX::XMFLOAT2& out_position);
	bool ApplyChainLightningDamage(int enemy_id, float damage);
	bool ConsumeCombatFeedback(CombatFeedback& out_feedback);
	int ApplyDashSlashDamage(const DirectX::XMFLOAT2& start, const DirectX::XMFLOAT2& end, float half_width,
	                         float damage);
	bool UpdateDashSlashAttacks(float delta_time);
	bool IsRoomDiscovered(int room_index);
	bool IsRoomCleared(int room_index);
	bool IsRoundCleared();
	bool HasPendingBossSpawn();
	bool ConsumeBossDefeatedEvent(BossDefeatedEvent& out_event);
	void CompleteBossDefeat();
	bool TryGetBossHealth(float& out_hit_point, float& out_max_hit_point);
	const char* GetBossDisplayName();
	void DrawSpawnTelegraphs();
	void Draw();
	void DrawProjectiles();
	int AppendPointLights(SpritePointLight* lights, int light_count, int capacity,
	                      const DirectX::XMFLOAT2& camera_position, const DirectX::XMFLOAT2& viewport_size);
	void RegisterColliders();
	void HandleCollisionHits(cChainLightning& chain_lightning);
	bool ConsumeEnemyBullet(int projectile_id, float& out_damage);
	void Deactivate(int enemy_id);
	bool IsActive(int enemy_id);
} // namespace GameEnemy

#endif // !GAME_ENEMY_H
