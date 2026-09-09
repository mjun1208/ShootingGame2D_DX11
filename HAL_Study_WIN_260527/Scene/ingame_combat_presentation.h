#ifndef INGAME_COMBAT_PRESENTATION_H
#define INGAME_COMBAT_PRESENTATION_H

#include <memory>

class cCamera;
class cChainLightning;

// 전투 중 화면 연출을 한곳에서 관리한다.
// 전투 상태는 GamePlayer와 GameEnemy가 관리하고, 이 클래스는 카메라와 화면 효과를 다룬다.
class IngameCombatPresentation final
{
  public:
	IngameCombatPresentation();
	~IngameCombatPresentation();

	IngameCombatPresentation(const IngameCombatPresentation&) = delete;
	IngameCombatPresentation& operator=(const IngameCombatPresentation&) = delete;

	bool Initialize();
	void Finalize();
	void Reset();

	bool UpdateHitStop(float delta_time);
	void ConsumeEnemyFeedback(cCamera& camera);
	void PresentPlayerDamage(float damage, cCamera& camera);
	void DrawScreenFlash() const;

	bool BeginPlayerDeath(cCamera& camera);
	bool UpdatePlayerDeath(float delta_time, cCamera& camera);
	void DrawPlayerDeath(int overlay_texture_id, cCamera& camera) const;
	bool IsPlayerDeathActive() const;

	bool TryBeginBossDeath(cCamera& camera, cChainLightning& chain_lightning);
	void UpdateBossDeath(float delta_time, cCamera& camera);
	bool IsBossDeathActive() const;

  private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};

#endif // INGAME_COMBAT_PRESENTATION_H
