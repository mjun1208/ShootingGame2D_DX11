#ifndef INGAME_SCENE_H
#define INGAME_SCENE_H

#include "boss_intro_presentation.h"
#include "camera.h"
#include "chain_lightning.h"
#include "chest.h"
#include "ingame_hud.h"
#include "ingame_menu_controller.h"
#include "round_portal.h"
#include "scene.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

class IngameScene final : public cScene
{
public:
	~IngameScene() override;

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
	void AdvanceToNextRound();
	void UpdateRoundTransition(float delta_time);
	void BeginDeathSequence();
	void UpdateDeathSequence(float delta_time);
	bool TryBeginBossDeathPresentation();
	void BeginBossDeathPresentation(
		const DirectX::XMFLOAT2& position,
		const DirectX::XMFLOAT2& draw_size);
	void UpdateBossDeathPresentation(float delta_time);
	float NextBossDeathRandom();
	bool IsRoundExitOpen() const;
	void HandlePauseAction(IngamePauseAction action);
	void ApplyAugment(const IngameAugmentSelection& selection);
	void ClearRewardChests();
	void SpawnClearedRoomRewardChests();
	void UpdateRewardChestRoomLock();

	cCamera m_Camera;
	cChainLightning m_ChainLightning;
	std::vector<std::unique_ptr<Chest>> m_RewardChests;
	std::vector<int> m_RewardChestRoomIndices;
	Chest* m_InitialWeaponChest{ nullptr };
	Chest* m_RoundExitRewardChest{ nullptr };
	int m_ChestLockedRoomIndex{ -1 };
	std::vector<bool> m_RewardedRooms;
	RoundPortal m_RoundPortal;
	IngameHud m_Hud;
	IngameMenuController m_MenuController;
	BossIntroPresentation m_BossIntro;
	DirectX::XMFLOAT2 m_AutoAimTarget{};
	int m_DeathOverlayTextureID{ -1 };
	int m_TimeStopActivateAudioID{ -1 };
	std::array<int, 6> m_BossDeathExplosionAudioIDs{
		-1, -1, -1, -1, -1, -1 };
	int m_BossDeathFinalAudioID{ -1 };
	float m_RoundElapsedTime{ 0.0f };
	float m_RunElapsedTime{ 0.0f };
	float m_QSkillCooldownRemaining{ 0.0f };
	float m_PlayerFireShakeCooldownRemaining{ 0.0f };
	int m_CurrentRound{ 1 };
	float m_FadeAlpha{ 1.0f };
	float m_DeathElapsedTime{ 0.0f };
	float m_DeathAnimationFinishedElapsed{ 0.0f };
	DirectX::XMFLOAT2 m_BossDeathPosition{};
	DirectX::XMFLOAT2 m_BossDeathDrawSize{};
	DirectX::XMFLOAT2 m_BossDeathCameraStartPosition{};
	DirectX::XMFLOAT2 m_BossDeathCameraTargetPosition{};
	float m_BossDeathElapsedTime{ 0.0f };
	float m_BossDeathCameraStartZoom{ 1.0f };
	std::uint32_t m_BossDeathRandomState{ 0xB055D34Du };
	int m_BossDeathExplosionCount{ 0 };
	std::size_t m_BossDeathExplosionAudioIndex{ 0 };
	RoundTransitionState m_TransitionState{ RoundTransitionState::FadingIn };
	bool m_IsDeathSequenceActive{ false };
	bool m_HasDeathAnimationStarted{ false };
	bool m_IsBossDeathPresentationActive{ false };
	bool m_HasBossDeathFinalExplosionPlayed{ false };
	bool m_ShowWorldMap{ false };
	bool m_HasAutoAimTarget{ false };
	bool m_HasEnteredExitRoom{ false };
	int m_LastAugmentLevel{ 1 };
};

#endif // !INGAME_SCENE_H
