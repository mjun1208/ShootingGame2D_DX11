#ifndef ENEMY_ATTACK_PATTERN_H
#define ENEMY_ATTACK_PATTERN_H

#include <DirectXMath.h>

#include <array>
#include <cstdint>

class cEnemy;
enum class MonsterType : std::uint8_t;

namespace EnemyAttackPattern
{
	struct DashAfterimagePath
	{
		DirectX::XMFLOAT2 Start{};
		DirectX::XMFLOAT2 End{};
		float Fade{ 0.0f };
	};

	void Initialize(
		int bone_texture_id,
		int dagger_texture_id,
		int axe_texture_id,
		int mage_projectile_texture_id,
		int warning_texture_id,
		int ground_effect_texture_id,
		int bone_throw_audio_id,
		const std::array<int, 3>& mage_fire_audio_ids,
		int warrior_slash_audio_id,
		int bat_dash_audio_id,
		int shaman_cast_audio_id);
	void Finalize();
	void Reset(std::uint32_t seed);
	void OnSpawn(int enemy_id, MonsterType type);
	void OnDeactivate(int enemy_id);
	void UpdateEnemy(
		int enemy_id,
		MonsterType type,
		cEnemy& enemy,
		float delta_time,
		const DirectX::XMFLOAT2& player_position,
		float visual_bottom_offset,
		float animation_elapsed);
	float GetAnimationElapsed(int enemy_id, float default_elapsed);
	bool GetAnimationFrameRegion(
		int enemy_id,
		int& out_frame_x,
		int& out_frame_y);
	float GetVisualAlpha(int enemy_id);
	bool IsTargetable(int enemy_id);
	bool IsBossAttackWindow(int enemy_id);
	int GetBossAttackVariant(int enemy_id);
	int GetDashAfterimagePaths(
		int enemy_id,
		DashAfterimagePath* out_paths,
		int capacity);
	void UpdateProjectiles(float delta_time);
	void DrawTelegraphs();
	void DrawProjectiles();
	void RegisterProjectileColliders(int owner_id_offset);
	bool ConsumeProjectile(int projectile_id, float& out_damage);
}

#endif // !ENEMY_ATTACK_PATTERN_H
