#ifndef INGAME_SCENE_H
#define INGAME_SCENE_H

#include "camera.h"
#include "chain_lightning.h"
#include "chest.h"
#include "procedural_map.h"
#include "round_portal.h"
#include "scene.h"

class IngameScene final : public cScene
{
public:
	bool Initialize() override;
	void Finalize() override;
	void Update(float delta_time) override;
	void Draw() override;

private:
	enum class RoundTransitionState
	{
		None,
		FadingOut,
		FadingIn,
	};

	void ResetRound(int global_round, bool regenerate_current);
	void BeginRoundTransition();
	void UpdateRoundTransition(float delta_time);
	void BeginDeathSequence();
	void UpdateDeathSequence(float delta_time);
	bool IsRoundExitOpen() const;

	cCamera m_Camera;
	cChainLightning m_ChainLightning;
	Chest m_TestChest;
	RoundPortal m_RoundPortal;
	DirectX::XMFLOAT2 m_AutoAimTarget{};
	int m_CrosshairTextureID{ -1 };
	int m_HealthBarEmptyTextureID{ -1 };
	int m_HealthBarFilledTextureID{ -1 };
	int m_ExperienceBarEmptyTextureID{ -1 };
	int m_ExperienceBarFilledTextureID{ -1 };
	int m_InteractionPromptTextureID{ -1 };
	int m_DeathOverlayTextureID{ -1 };
	float m_FireCooldown{ 0.0f };
	float m_RoundElapsedTime{ 0.0f };
	float m_RunElapsedTime{ 0.0f };
	int m_CurrentRound{ 1 };
	float m_FadeAlpha{ 1.0f };
	float m_DeathElapsedTime{ 0.0f };
	float m_DeathAnimationFinishedElapsed{ 0.0f };
	RoundTransitionState m_TransitionState{ RoundTransitionState::FadingIn };
	bool m_IsDeathSequenceActive{ false };
	bool m_HasDeathAnimationStarted{ false };
	bool m_ShowWorldMap{ false };
	bool m_HasAutoAimTarget{ false };
};

#endif // !INGAME_SCENE_H
