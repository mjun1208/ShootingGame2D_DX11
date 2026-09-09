#ifndef INGAME_HUD_H
#define INGAME_HUD_H

#include <DirectXMath.h>

#include <memory>

class IInteractable;
enum class BulletType;

class IngameHud final
{
  public:
	IngameHud();
	~IngameHud();

	IngameHud(const IngameHud&) = delete;
	IngameHud& operator=(const IngameHud&) = delete;

	bool Initialize(const DirectX::XMFLOAT2& viewport_size);
	void Finalize();
	void ResetBossPresentation();
	void ShowWeaponUnlock(BulletType type);
	bool IsWeaponUnlockPopupOpen() const;
	void UpdateWeaponUnlockPopup();

	void Draw(float q_skill_cooldown_remaining, bool show_world_map, bool boss_intro_active, bool has_auto_aim_target,
	          const DirectX::XMFLOAT2& auto_aim_screen_position);
	void DrawInteractionPrompt(const IInteractable& interactable, const DirectX::XMFLOAT2& player_position,
	                           float elapsed_time) const;

	static float GetQSkillCooldownDuration();

  private:
	struct Impl;
	std::unique_ptr<Impl> m_Impl;
};

#endif // !INGAME_HUD_H
