#ifndef ENEMY_ATTACK_PATTERN_H
#define ENEMY_ATTACK_PATTERN_H

#include "monster_type.h"

#include <DirectXMath.h>

#include <array>

class cEnemy;

namespace EnemyAttackPattern
{
	struct DashAfterimagePath
	{
		DirectX::XMFLOAT2 Start{};
		DirectX::XMFLOAT2 End{};
		float Fade{ 0.0f };
	};

	struct Resources
	{
		int BoneTextureID{ -1 };
		int DaggerTextureID{ -1 };
		int AxeTextureID{ -1 };
		int MageProjectileTextureID{ -1 };
		int WarningTextureID{ -1 };
		int GroundEffectTextureID{ -1 };
		int BoneThrowAudioID{ -1 };
		std::array<int, 3> MageFireAudioIDs{ -1, -1, -1 };
		int WarriorSlashAudioID{ -1 };
		int BatDashAudioID{ -1 };
		int ShamanCastAudioID{ -1 };
	};

	void Initialize(const Resources& resources);
	void Finalize();
	void Reset();
	void OnSpawn(int enemy_id, MonsterType type);
	void OnDeactivate(int enemy_id);
	void UpdateEnemy(int enemy_id, MonsterType type, cEnemy& enemy, float delta_time,
	                 const DirectX::XMFLOAT2& player_position, float visual_bottom_offset, float animation_elapsed);
	float GetAnimationElapsed(int enemy_id, float default_elapsed);
	bool GetAnimationFrameRegion(int enemy_id, int& out_frame_x, int& out_frame_y);
	float GetVisualAlpha(int enemy_id);
	bool IsTargetable(int enemy_id);
	bool IsBossAttackWindow(int enemy_id);
	bool IsCthulhuDashing(int enemy_id);
	int GetBossAttackVariant(int enemy_id);
	int GetDashAfterimagePaths(int enemy_id, DashAfterimagePath* out_paths, int capacity);
	void UpdateProjectiles(float delta_time);
	void DrawTelegraphs();
	void DrawProjectiles();
	void RegisterProjectileColliders(int owner_id_offset);
	bool ConsumeProjectile(int projectile_id, float& out_damage);
} // namespace EnemyAttackPattern

#endif // !ENEMY_ATTACK_PATTERN_H
