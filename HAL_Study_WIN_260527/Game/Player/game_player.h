#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <DirectXMath.h>

namespace GamePlayer
{
	void Initialize();
	void Finalize();
	void BeginEmpoweredDashMode();
	void EndEmpoweredDashMode();
	void RequestDash();
	void UpdateEmpoweredDashAttack(float delta_time);
	void SetSprinting(bool is_sprinting);
	bool IsEmpoweredDashModeActive();
	int GetEmpoweredDashCharges();
	void Update(float delta_time);
	void SetPosition(const DirectX::XMFLOAT2& position);
	void SetAimTarget(const DirectX::XMFLOAT2& world_position);
	void ApplyDamage(float damage);
	bool ConsumeDamageFeedback(float& out_damage);
	float Heal(float amount);
	void AddExperience(int experience);
	void IncreaseMaxHitPoint(float amount);
	void MultiplyMoveSpeed(float multiplier);
	float GetHitPoint();
	float GetMaxHitPoint();
	int GetLevel();
	int GetExperience();
	int GetExperienceToNextLevel();
	void PrepareDeathSequence();
	void BeginDeathAnimation();
	void UpdateDeathAnimation(float delta_time);
	bool IsDeathAnimationFinished();
	void RegisterCollider();
	void HandleCollisionHits();
	void Draw();
	void DrawMapMarker(const DirectX::XMFLOAT2& map_origin, float world_scale, bool expanded);
	DirectX::XMFLOAT2 GetAimDirection();
	DirectX::XMFLOAT2 GetPosition();
} // namespace GamePlayer

#endif // !GAME_PLAYER_H
