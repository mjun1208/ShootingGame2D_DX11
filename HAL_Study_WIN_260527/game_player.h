#ifndef GAME_PLAYER_H
#define GAME_PLAYER_H

#include <DirectXMath.h>
 
namespace GamePlayer
{
	void Initialize();
	void Finalize();
	void RequestDash();
	void Update(float delta_time);
	void SetPosition(const DirectX::XMFLOAT2& position);
	void SetAimTarget(const DirectX::XMFLOAT2& world_position);
	void ApplyDamage(float damage);
	void AddExperience(int experience);
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
	void DrawMapMarker(
		const DirectX::XMFLOAT2& map_origin,
		float world_scale,
		bool expanded);
	DirectX::XMFLOAT2 GetAimDirection();
	DirectX::XMFLOAT2 GetPosition();
}

#endif // !GAME_PLAYER_H
